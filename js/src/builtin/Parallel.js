/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Shared utility functions for parallel operations in `Array.js`
// and `TypedObject.js`.


/**
 * Determine the number and size of slices.
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

  return { shift: shift, statuses: new Uint8Array(count), lastSequentialId: 0 };
}

/**
 * Reset the status array of the slices info object.
 */
function SlicesInfoClearStatuses(info) {
  var statuses = info.statuses;
  var length = statuses.length;
  for (var i = 0; i < length; i++)
    UnsafePutElements(statuses, i, 0);
  info.lastSequentialId = 0;
}

/**
 * Compute the slice such that all slices before it (but not including it) are
 * completed.
 */
function NextSequentialSliceId(info, doneMarker) {
  var statuses = info.statuses;
  var length = statuses.length;
  for (var i = info.lastSequentialId; i < length; i++) {
    if (statuses[i] === SLICE_STATUS_DONE)
      continue;
    info.lastSequentialId = i;
    return i;
  }
  return doneMarker == undefined ? length : doneMarker;
}

/**
 * Determinism-preserving bounds function.
 */
function ShrinkLeftmost(info) {
  return function () {
    return [NextSequentialSliceId(info), SLICE_COUNT(info)]
  };
}


