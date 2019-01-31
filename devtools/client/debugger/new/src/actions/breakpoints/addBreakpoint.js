/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { isOriginalId } from "devtools-source-map";
import {
  locationMoved,
  breakpointExists,
  assertBreakpoint,
  createBreakpoint,
  getASTLocation,
  assertLocation,
  makeBreakpointId,
  makeSourceActorLocation
} from "../../utils/breakpoint";
import { PROMISE } from "../utils/middleware/promise";
import {
  getSource,
  getSourceActors,
  getSymbols,
} from "../../selectors";
import { getGeneratedLocation } from "../../utils/source-maps";
import { getTextAtPosition } from "../../utils/source";
import { recordEvent } from "../../utils/telemetry";

import type {
  BreakpointOptions,
  Breakpoint,
  SourceLocation
} from "../../types";
import type { ThunkArgs } from "../types";

async function addBreakpointPromise(getState, client, sourceMaps, breakpoint) {
  const state = getState();
  const source = getSource(state, breakpoint.location.sourceId);

  if (!source) {
    throw new Error(`Unable to find source: ${breakpoint.location.sourceId}`);
  }

  const location = {
    ...breakpoint.location,
    sourceId: source.id,
    sourceUrl: source.url
  };

  const generatedLocation = await getGeneratedLocation(
    state,
    source,
    location,
    sourceMaps
  );

  const generatedSource = getSource(state, generatedLocation.sourceId);

  if (!generatedSource) {
    throw new Error(
      `Unable to find generated source: ${generatedLocation.sourceId}`
    );
  }

  assertLocation(location);
  assertLocation(generatedLocation);

  if (breakpointExists(state, location)) {
    const newBreakpoint = { ...breakpoint, location, generatedLocation };
    assertBreakpoint(newBreakpoint);
    return { breakpoint: newBreakpoint };
  }

  const sourceActors = getSourceActors(state, generatedSource.id);
  const newGeneratedLocation = { ...generatedLocation };

  for (const sourceActor of sourceActors) {
    const sourceActorLocation = makeSourceActorLocation(
      sourceActor,
      generatedLocation
    );
    const { actualLocation } = await client.setBreakpoint(
      sourceActorLocation,
      breakpoint.options,
      isOriginalId(location.sourceId)
    );
    newGeneratedLocation.line = actualLocation.line;
    newGeneratedLocation.column = actualLocation.column;
  }

  const newLocation = await sourceMaps.getOriginalLocation(
    newGeneratedLocation
  );

  const symbols = getSymbols(getState(), source);
  const astLocation = await getASTLocation(source, symbols, newLocation);

  const originalText = getTextAtPosition(source, location);
  const text = getTextAtPosition(generatedSource, newGeneratedLocation);

  const newBreakpoint = {
    id: makeBreakpointId(generatedLocation),
    disabled: false,
    loading: false,
    options: breakpoint.options,
    location: newLocation,
    astLocation,
    generatedLocation: newGeneratedLocation,
    text,
    originalText
  };

  assertBreakpoint(newBreakpoint);

  const previousLocation = locationMoved(location, newLocation)
    ? location
    : null;

  return {
    breakpoint: newBreakpoint,
    previousLocation
  };
}

export function addHiddenBreakpoint(location: SourceLocation) {
  return ({ dispatch }: ThunkArgs) => {
    return dispatch(addBreakpoint(location, { hidden: true }));
  };
}

export function enableBreakpoint(breakpoint: Breakpoint) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    if (breakpoint.loading) {
      return;
    }

    // To instantly reflect in the UI, we optimistically enable the breakpoint
    const enabledBreakpoint = { ...breakpoint, disabled: false };

    return dispatch({
      type: "ENABLE_BREAKPOINT",
      breakpoint: enabledBreakpoint,
      [PROMISE]: addBreakpointPromise(getState, client, sourceMaps, breakpoint)
    });
  };
}

/**
 * Add a new breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 * @param {BreakpointOptions} options Any options for the new breakpoint.
 */

export function addBreakpoint(
  location: SourceLocation,
  options: BreakpointOptions = {}
) {
  return ({ dispatch, getState, sourceMaps, client }: ThunkArgs) => {
    recordEvent("add_breakpoint");

    const breakpoint = createBreakpoint(location, options);

    return dispatch({
      type: "ADD_BREAKPOINT",
      breakpoint,
      [PROMISE]: addBreakpointPromise(getState, client, sourceMaps, breakpoint)
    });
  };
}
