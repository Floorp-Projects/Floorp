/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  breakpointExists,
  assertBreakpoint,
  createBreakpoint,
  getASTLocation,
  assertLocation,
  makeBreakpointId,
  makeBreakpointLocation
} from "../../utils/breakpoint";
import { PROMISE } from "../utils/middleware/promise";
import {
  getSource,
  getSourceActors,
  getSymbols,
  getFirstVisibleBreakpointPosition
} from "../../selectors";
import { getGeneratedLocation } from "../../utils/source-maps";
import { getTextAtPosition } from "../../utils/source";
import { recordEvent } from "../../utils/telemetry";
import { features } from "../../utils/prefs";
import { setBreakpointPositions } from "./breakpointPositions";

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
    return newBreakpoint;
  }

  const breakpointLocation = makeBreakpointLocation(getState(), generatedLocation);
  await client.setBreakpoint(breakpointLocation, breakpoint.options);

  const symbols = getSymbols(getState(), source);
  const astLocation = await getASTLocation(source, symbols, location);

  const originalText = getTextAtPosition(source, location);
  const text = getTextAtPosition(generatedSource, generatedLocation);

  const newBreakpoint = {
    id: makeBreakpointId(generatedLocation),
    disabled: false,
    loading: false,
    options: breakpoint.options,
    location,
    astLocation,
    generatedLocation,
    text,
    originalText
  };

  assertBreakpoint(newBreakpoint);

  return newBreakpoint;
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

export function addBreakpoint(
  location: SourceLocation,
  options: BreakpointOptions = {}
) {
  return async ({ dispatch, getState, sourceMaps, client }: ThunkArgs) => {
    recordEvent("add_breakpoint");
    let breakpointPosition = location;
    if (features.columnBreakpoints && location.column === undefined) {
      await dispatch(setBreakpointPositions(location));
      breakpointPosition = getFirstVisibleBreakpointPosition(
        getState(),
        location
      );
    }

    if (!breakpointPosition) {
      return;
    }

    const breakpoint = createBreakpoint(breakpointPosition, options);

    return dispatch({
      type: "ADD_BREAKPOINT",
      breakpoint,
      [PROMISE]: addBreakpointPromise(getState, client, sourceMaps, breakpoint)
    });
  };
}
