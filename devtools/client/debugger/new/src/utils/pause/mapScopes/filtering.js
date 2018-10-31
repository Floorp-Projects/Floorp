/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

function findInsertionLocation<T>(
  array: Array<T>,
  callback: T => number
): number {
  let left = 0;
  let right = array.length;
  while (left < right) {
    const mid = Math.floor((left + right) / 2);
    const item = array[mid];

    const result = callback(item);
    if (result === 0) {
      left = mid;
      break;
    }
    if (result >= 0) {
      right = mid;
    } else {
      left = mid + 1;
    }
  }

  // Ensure the value is the start of any set of matches.
  let i = left;
  if (i < array.length) {
    while (i >= 0 && callback(array[i]) >= 0) {
      i--;
    }
    return i + 1;
  }

  return i;
}

export function filterSortedArray<T>(
  array: Array<T>,
  callback: T => number
): Array<T> {
  const start = findInsertionLocation(array, callback);

  const results = [];
  for (let i = start; i < array.length && callback(array[i]) === 0; i++) {
    results.push(array[i]);
  }

  return results;
}
