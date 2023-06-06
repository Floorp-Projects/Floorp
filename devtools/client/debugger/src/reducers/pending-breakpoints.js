/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Pending breakpoints reducer.
 *
 * Pending breakpoints are a more lightweight version compared to regular breakpoints objects.
 * They are meant to be persisted across Firefox restarts and stored into async-storage.
 * This reducer data is saved into asyncStore from bootstrap.js and restored from main.js.
 *
 * The main difference with pending breakpoints is that we only save breakpoints
 * against source with an URL as only them can be restored. (source IDs are different across reloads).
 * The second difference is that we don't store the whole source object but only the source URL.
 */

import { isPrettyURL } from "../utils/source";
import assert from "../utils/assert";

function update(state = {}, action) {
  switch (action.type) {
    case "SET_BREAKPOINT":
      if (action.status === "start") {
        return setBreakpoint(state, action.breakpoint);
      }
      return state;

    case "REMOVE_BREAKPOINT":
      if (action.status === "start") {
        return removeBreakpoint(state, action.breakpoint);
      }
      return state;

    case "REMOVE_PENDING_BREAKPOINT":
      return removePendingBreakpoint(state, action.pendingBreakpoint);

    case "CLEAR_BREAKPOINTS": {
      return {};
    }
  }

  return state;
}

function shouldBreakpointBePersisted(breakpoint) {
  // We only save breakpoint for source with URL.
  // Source without URL can only be identified via their source actor ID
  // which isn't persisted across reloads.
  return !breakpoint.options.hidden && breakpoint.location.source.url;
}

function setBreakpoint(state, breakpoint) {
  if (!shouldBreakpointBePersisted(breakpoint)) {
    return state;
  }

  const id = makeIdFromBreakpoint(breakpoint);
  const pendingBreakpoint = createPendingBreakpoint(breakpoint);

  return { ...state, [id]: pendingBreakpoint };
}

function removeBreakpoint(state, breakpoint) {
  if (!shouldBreakpointBePersisted(breakpoint)) {
    return state;
  }

  const id = makeIdFromBreakpoint(breakpoint);
  state = { ...state };

  delete state[id];
  return state;
}

function removePendingBreakpoint(state, pendingBreakpoint) {
  const id = makeIdFromPendingBreakpoint(pendingBreakpoint);
  state = { ...state };

  delete state[id];
  return state;
}

/**
 * Return a unique identifier for a given breakpoint,
 * using its original location, or for pretty-printed sources,
 * its generated location.
 *
 * @param {Object} breakpoint
 */
function makeIdFromBreakpoint(breakpoint) {
  const location = isPrettyURL(breakpoint.location.source.url)
    ? breakpoint.generatedLocation
    : breakpoint.location;

  const { source, line, column } = location;
  const sourceUrlString = source.url || "";
  const columnString = column || "";

  return `${sourceUrlString}:${line}:${columnString}`;
}

function makeIdFromPendingBreakpoint(pendingBreakpoint) {
  const { sourceUrl, line, column } = pendingBreakpoint.location;
  const sourceUrlString = sourceUrl || "";
  const columnString = column || "";

  return `${sourceUrlString}:${line}:${columnString}`;
}

/**
 * Convert typical debugger frontend location (created via location.js:createLocation)
 * to a more lightweight flavor of it which will be stored in async storage.
 */
function createPendingLocation(location) {
  assert(location.hasOwnProperty("line"), "location must have a line");
  assert(location.hasOwnProperty("column"), "location must have a column");

  const { source, line, column } = location;
  assert(source.url !== undefined, "pending location must have a source url");
  return { sourceUrl: source.url, line, column };
}

/**
 * Create a new pending breakpoint, which is a more lightweight version of the regular breakpoint object.
 */
function createPendingBreakpoint(bp) {
  return {
    options: bp.options,
    disabled: bp.disabled,
    location: createPendingLocation(bp.location),
    generatedLocation: createPendingLocation(bp.generatedLocation),
  };
}

export default update;
