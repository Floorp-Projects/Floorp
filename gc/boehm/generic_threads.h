/*
	generic_threads.c
	
	A module that permits clients of the GC to supply callback functions
	for thread stack scanning.
	
	by Patrick C. Beard.
 */

#ifndef __GENERIC_THREADS__
#define __GENERIC_THREADS__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Passed to a thread marking function, to allow it to mark a range of memory.
 */
typedef void (*GC_mark_range_proc) (char* begin, char* end);

/**
 * Generic all stack marking function.
 */
typedef void (*GC_generic_mark_all_stacks_proc) (GC_mark_range_proc marker);

extern GC_generic_mark_all_stacks_proc GC_generic_mark_all_stacks;

/**
 * Generic mutual exclusion facility.
 */
typedef void (*GC_generic_proc) (void* mutex);

extern void* GC_generic_mutex;
extern GC_generic_proc GC_generic_locker;
extern GC_generic_proc GC_generic_unlocker;
extern GC_generic_proc GC_generic_stopper;
extern GC_generic_proc GC_generic_starter;

extern void GC_generic_init_threads(GC_generic_mark_all_stacks_proc mark_all_stacks,
									void* mutex,
									GC_generic_proc locker, GC_generic_proc unlocker,
									GC_generic_proc stopper, GC_generic_proc starter);
									
#ifdef __cplusplus
}
#endif

#endif /* __GENERIC_THREADS__ */
