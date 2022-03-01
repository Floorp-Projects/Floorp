/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

const mItems = [];

const now = Date.now();
const mInitialTime = now;
let mLastTime    = now;
let mDeltaBetweenLastItem = 0;

export function add(label) {
  const now = Date.now();
  mItems.push({
    label: label,
    delta: now - mLastTime
  });
  mDeltaBetweenLastItem = now - mInitialTime;
  mLastTime = now;
}

export async function addAsync(label, asyncTask) {
  const start = Date.now();
  if (typeof asyncTask == 'function')
    asyncTask = asyncTask();
  return asyncTask.then(result => {
    mItems.push({
      label: `(async) ${label}`,
      delta: Date.now() - start,
      async: true
    });
    return result;
  });
}

export function toString() {
  const logs = mItems.map(item => `${item.delta || 0}: ${item.label}`);
  return `total ${mDeltaBetweenLastItem} msec\n${logs.join('\n')}`;
}

