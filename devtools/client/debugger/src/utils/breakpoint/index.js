/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getBreakpoint,
  getSource,
  getSourceActorsForSource,
} from "../../selectors";
import { isGenerated } from "../source";
import { sortSelectedLocations } from "../location";
import assert from "../assert";
import { features } from "../prefs";

export * from "./astBreakpointLocation";
export * from "./breakpointPositions";

import type {
  Source,
  SourceActor,
  SourceLocation,
  SourceActorLocation,
  PendingLocation,
  Breakpoint,
  BreakpointLocation,
  PendingBreakpoint,
} from "../../types";

import type { State } from "../../reducers/types";

// Return the first argument that is a string, or null if nothing is a
// string.
export function firstString(...args: string[]) {
  for (const arg of args) {
    if (typeof arg === "string") {
      return arg;
    }
  }
  return null;
}

// The ID for a Breakpoint is derived from its location in its Source.
export function makeBreakpointId(location: SourceLocation) {
  const { sourceId, line, column } = location;
  const columnString = column || "";
  return `${sourceId}:${line}:${columnString}`;
}

export function getLocationWithoutColumn(location: SourceLocation) {
  const { sourceId, line } = location;
  return `${sourceId}:${line}`;
}

type AnySourceLocation = SourceLocation | PendingLocation;

export function makePendingLocationId(location: AnySourceLocation) {
  assertPendingLocation(location);
  const { sourceUrl, line, column } = location;
  const sourceUrlString = sourceUrl || "";
  const columnString = column || "";

  return `${sourceUrlString}:${line}:${columnString}`;
}

export function makeBreakpointLocation(
  state: State,
  location: SourceLocation
): BreakpointLocation {
  const source = getSource(state, location.sourceId);
  if (!source) {
    throw new Error("no source");
  }
  const breakpointLocation: any = {
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

export function makeSourceActorLocation(
  sourceActor: SourceActor,
  location: SourceLocation
) {
  return {
    sourceActor,
    line: location.line,
    column: location.column,
  };
}

// The ID for a BreakpointActor is derived from its location in its SourceActor.
export function makeBreakpointActorId(location: SourceActorLocation) {
  const { sourceActor, line, column } = location;
  const columnString = column || "";
  return `${(sourceActor: any)}:${line}:${columnString}`;
}

export function assertBreakpoint(breakpoint: Breakpoint) {
  assertLocation(breakpoint.location);
  assertLocation(breakpoint.generatedLocation);
}

export function assertPendingBreakpoint(pendingBreakpoint: PendingBreakpoint) {
  assertPendingLocation(pendingBreakpoint.location);
  assertPendingLocation(pendingBreakpoint.generatedLocation);
}

export function assertLocation(location: SourceLocation) {
  assertPendingLocation(location);
  const { sourceId } = location;
  assert(!!sourceId, "location must have a source id");
}

export function assertPendingLocation(location: PendingLocation) {
  assert(!!location, "location must exist");

  const { sourceUrl } = location;

  // sourceUrl is null when the source does not have a url
  assert(sourceUrl !== undefined, "location must have a source url");
  assert(location.hasOwnProperty("line"), "location must have a line");
  assert(
    location.hasOwnProperty("column") != null,
    "location must have a column"
  );
}

// syncing
export function breakpointAtLocation(
  breakpoints: Breakpoint[],
  { line, column }: SourceLocation
) {
  return breakpoints.find(breakpoint => {
    const sameLine = breakpoint.location.line === line;
    if (!sameLine) {
      return false;
    }

    // NOTE: when column breakpoints are disabled we want to find
    // the first breakpoint
    if (!features.columnBreakpoints) {
      return true;
    }

    return breakpoint.location.column === column;
  });
}

export function breakpointExists(state: State, location: SourceLocation) {
  const currentBp = getBreakpoint(state, location);
  return currentBp && !currentBp.disabled;
}

export function createXHRBreakpoint(
  path: string,
  method: string,
  overrides?: Object = {}
) {
  const properties = {
    path,
    method,
    disabled: false,
    loading: false,
    text: L10N.getFormatStr("xhrBreakpoints.item.label", path),
  };

  return { ...properties, ...overrides };
}

function createPendingLocation(location: PendingLocation) {
  const { sourceUrl, line, column } = location;
  return { sourceUrl, line, column };
}

export function createPendingBreakpoint(bp: Breakpoint) {
  const pendingLocation = createPendingLocation(bp.location);
  const pendingGeneratedLocation = createPendingLocation(bp.generatedLocation);

  assertPendingLocation(pendingLocation);

  return {
    options: bp.options,
    disabled: bp.disabled,
    location: pendingLocation,
    astLocation: bp.astLocation,
    generatedLocation: pendingGeneratedLocation,
  };
}

export function getSelectedText(
  breakpoint: Breakpoint,
  selectedSource: Source
) {
  return selectedSource && isGenerated(selectedSource)
    ? breakpoint.text
    : breakpoint.originalText;
}

export function sortSelectedBreakpoints(
  breakpoints: Array<Breakpoint>,
  selectedSource: ?Source
): Array<Breakpoint> {
  return sortSelectedLocations(breakpoints, selectedSource);
}
