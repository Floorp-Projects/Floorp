/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSourceActorsForSource } from "../../selectors";
import { isGenerated } from "../source";
import { sortSelectedLocations } from "../location";
export * from "./breakpointPositions";

// The ID for a Breakpoint is derived from its location in its Source.
export function makeBreakpointId(location) {
  const { source, line, column } = location;
  const columnString = column || "";
  return `${source.id}:${line}:${columnString}`;
}

export function makeBreakpointServerLocationId(breakpointServerLocation) {
  const { sourceUrl, sourceId, line, column } = breakpointServerLocation;
  const sourceUrlOrId = sourceUrl || sourceId;
  const columnString = column || "";

  return `${sourceUrlOrId}:${line}:${columnString}`;
}

// Map frontend options to a more restricted subset of what
// the server supports. For example frontend uses `hidden` attribute
// which isn't meant to be passed to the server.
// (note that protocol.js specification isn't enough to filter attributes,
//  all primitive attributes will be passed as-is)
export function makeBreakpointServerOptions(options) {
  return { logValue: options.logValue, condition: options.condition };
}

/**
 * Create a location object to set a breakpoint on the server.
 *
 * Debugger location objects includes a source and sourceActor attributes
 * whereas the server don't need them and instead only need either
 * the source URL -or- a precise source actor ID.
 */
export function makeBreakpointServerLocation(state, location) {
  const source = location.source;
  if (!source) {
    throw new Error("Missing 'source' attribute on location object");
  }
  const breakpointLocation = {
    line: location.line,
    column: location.column,
  };
  if (source.url) {
    breakpointLocation.sourceUrl = source.url;
  } else {
    breakpointLocation.sourceId = getSourceActorsForSource(
      state,
      source.id
    )[0].id;
  }
  return breakpointLocation;
}

export function createXHRBreakpoint(path, method, overrides = {}) {
  const properties = {
    path,
    method,
    disabled: false,
    loading: false,
    text: L10N.getFormatStr("xhrBreakpoints.item.label", path),
  };

  return { ...properties, ...overrides };
}

export function getSelectedText(breakpoint, selectedSource) {
  return !!selectedSource && isGenerated(selectedSource)
    ? breakpoint.text
    : breakpoint.originalText;
}

export function sortSelectedBreakpoints(breakpoints, selectedSource) {
  return sortSelectedLocations(breakpoints, selectedSource);
}
