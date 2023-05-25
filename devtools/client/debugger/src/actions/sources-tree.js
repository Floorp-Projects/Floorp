/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export function setExpandedState(expanded) {
  return { type: "SET_EXPANDED_STATE", expanded };
}

export function focusItem(item) {
  return { type: "SET_FOCUSED_SOURCE_ITEM", item };
}
