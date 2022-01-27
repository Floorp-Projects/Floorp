/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export function getActiveEventListeners(state) {
  return state.eventListenerBreakpoints.active;
}

export function getEventListenerBreakpointTypes(state) {
  return state.eventListenerBreakpoints.categories;
}

export function getEventListenerExpanded(state) {
  return state.eventListenerBreakpoints.expanded;
}

export function shouldLogEventBreakpoints(state) {
  return state.eventListenerBreakpoints.logEventBreakpoints;
}
