/*
  generic_threads.c
  
  A module that permits clients of the GC to supply callback functions
  for thread stack scanning.
  
  by Patrick C. Beard.
 */

#include "generic_threads.h"
# include "gc_priv.h"
# include "gc_mark.h"

static void mark_range(char* begin, char* end)
{
  /* use the mark stack API, which will do more sanity checking. */
  GC_push_all(begin, end);
  /*
  while (begin < end) {
    GC_PUSH_ONE_STACK(*(word*)begin, 0);
    begin += ALIGNMENT;
  }
  */
}

/*
	Until a client installs a stack marking routine, this will mark the
	current stack. This is crucial to keep data live during program
	startup.
 */
static void default_mark_all_stacks(GC_mark_range_proc marker)
{
#ifdef STACK_GROWS_DOWN
  mark_range(GC_approx_sp(), GC_get_stack_base());
#else
  mark_range(GC_get_stack_base(), GC_approx_sp());
#endif
}

static void default_proc(void* mutex) {}

GC_generic_mark_all_stacks_proc GC_generic_mark_all_stacks = &default_mark_all_stacks;
void* GC_generic_mutex = NULL;
GC_generic_proc GC_generic_locker = &default_proc;
GC_generic_proc GC_generic_unlocker = &default_proc;
GC_generic_proc GC_generic_stopper = &default_proc;
GC_generic_proc GC_generic_starter = &default_proc;

void GC_generic_init_threads(GC_generic_mark_all_stacks_proc mark_all_stacks,
			     void* mutex,
			     GC_generic_proc locker, GC_generic_proc unlocker,
			     GC_generic_proc stopper, GC_generic_proc starter)
{
  GC_generic_mark_all_stacks = mark_all_stacks;
  GC_generic_mutex = mutex;
  GC_generic_locker = locker;
  GC_generic_unlocker = unlocker;
  GC_generic_stopper = stopper;
  GC_generic_starter = starter;
	
  GC_dont_expand = 1;
  // GC_set_max_heap_size(20L * 1024L * 1024L);
}

#if !defined(WIN32_THREADS) && !defined(LINUX_THREADS)
void GC_push_all_stacks()
{
  GC_generic_mark_all_stacks(&mark_range);
}

void GC_stop_world()
{
  GC_generic_stopper(GC_generic_mutex);
}

void GC_start_world()
{
  GC_generic_starter(GC_generic_mutex);
}
#endif /* WIN32_THREADS */
