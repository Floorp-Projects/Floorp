/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export function getBlackBoxRanges(state) {
  return state.sourceBlackBox.blackboxedRanges;
}

export function isSourceBlackBoxed(state, source) {
  // Only sources with a URL can be blackboxed.
  if (!source.url) {
    return false;
  }
  return state.sourceBlackBox.blackboxedSet.has(source.url);
}
