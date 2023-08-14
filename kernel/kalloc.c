// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct Linknum
{
  struct spinlock lock;
  int count;
  /* data */
};

struct Linknum linknum[PHYSTOP/PGSIZE];

void
kinit()
{
  for(int i=0;i<PHYSTOP/PGSIZE;i++)
  {
    initlock(&(linknum[i].lock),"klinknum");
  }
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    acquire(&(linknum[(uint64)p/PGSIZE].lock));
    linknum[(uint64)p/PGSIZE].count=1;
    release(&(linknum[(uint64)p/PGSIZE].lock));
    kfree(p);
  }
    
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  uint64 pi=(uint64)pa/PGSIZE;
  acquire(&(linknum[pi].lock));
  linknum[pi].count-=1;
  if(linknum[pi].count>0)
  {
    release(&(linknum[pi].lock));
    return;
  }
  release(&(linknum[pi].lock));

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
  {
    uint64 pi = (uint64)r/PGSIZE;
    acquire(&(linknum[pi].lock));
    linknum[pi].count=1;
    release(&(linknum[pi].lock));

    kmem.freelist = r->next;
  }
    
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}


int get_linknum(uint64 pa)
{
  uint64 pi=(uint64)pa/PGSIZE;
  acquire(&(linknum[pi].lock));
  int n=linknum[pi].count;
  release(&(linknum[pi].lock));
  return n;
}

int add_link(uint64 pa)
{
  uint64 pi=(uint64)pa/PGSIZE;
  acquire(&(linknum[pi].lock));
  linknum[pi].count+=1;
  release(&(linknum[pi].lock));
  return 1;
}

