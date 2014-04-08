/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Shared utility functions for and macros parallel operations in `Array.js`
// and `TypedObject.js`.

#ifdef ENABLE_PARALLEL_JS

/* The mode asserts options object. */
#define TRY_PARALLEL(MODE) \
  ((!MODE || MODE.mode !== "seq"))
#define ASSERT_SEQUENTIAL_IS_OK(MODE) \
  do { if (MODE) AssertSequentialIsOK(MODE) } while(false)

/**
 * The ParallelSpew intrinsic is only defined in debug mode, so define a dummy
 * if debug is not on.
 */
#ifndef DEBUG
#define ParallelSpew(args)
#endif

#define MAX_SLICE_SHIFT 6
#define MAX_SLICE_SIZE 64
#define MAX_SLICES_PER_WORKER 8

/**
 * Macros to help compute the start and end indices of slices based on id. Use
 * with the object returned by ComputeSliceInfo.
 */
#define SLICE_START_INDEX(shift, id) \
    (id << shift)
#define SLICE_END_INDEX(shift, start, length) \
    std_Math_min(start + (1 << shift), length)

/**
 * ForkJoinGetSlice acts as identity when we are not in a parallel section, so
 * pass in the next sequential value when we are in sequential mode. The
 * reason for this odd API is because intrinsics *need* to be called during
 * ForkJoin's warmup to fill the TI info.
 */
#define GET_SLICE(sliceStart, sliceEnd, id) \
    ((id = ForkJoinGetSlice((InParallelSection() ? -1 : sliceStart++) | 0)) < sliceEnd)

/**
 * Determine the number and size of slices. The info object has the following
 * properties:
 *
 *  - shift: amount to shift by to compute indices
 *  - count: number of slices
 */
function ComputeSlicesInfo(length) {
  var count = length >>> MAX_SLICE_SHIFT;
  var numWorkers = ForkJoinNumWorkers();
  if (count < numWorkers)
    count = numWorkers;
  else if (count >= numWorkers * MAX_SLICES_PER_WORKER)
    count = numWorkers * MAX_SLICES_PER_WORKER;

  // Round the slice size to be a power of 2.
  var shift = std_Math_max(std_Math_log2(length / count) | 0, 1);

  // Recompute count with the rounded size.
  count = length >>> shift;
  if (count << shift !== length)
    count += 1;

  return { shift: shift, count: count };
}

#endif // ENABLE_PARALLEL_JS
