/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { setBreakpointPositions } from "./breakpointPositions";
import {
  createBreakpoint,
  assertBreakpoint,
  assertPendingBreakpoint,
  findFunctionByName,
  findPosition,
  makeBreakpointLocation
} from "../../utils/breakpoint";

import { getTextAtPosition } from "../../utils/source";
import { comparePosition } from "../../utils/location";

import { originalToGeneratedId, isOriginalId } from "devtools-source-map";
import { getSource, getBreakpointsList } from "../../selectors";
import { removeBreakpoint } from ".";

import type { ThunkArgs, Action } from "../types";

import type {
  SourceLocation,
  ASTLocation,
  PendingBreakpoint,
  SourceId,
  Breakpoint
} from "../../types";

type BreakpointSyncData = {
  previousLocation: SourceLocation,
  breakpoint: ?Breakpoint
};

async function findBreakpointPosition(
  { getState, dispatch },
  location: SourceLocation
) {
  const positions = await dispatch(setBreakpointPositions(location.sourceId));
  const position = findPosition(positions, location);
  return position && position.generatedLocation;
}

async function findNewLocation(
  { name, offset, index }: ASTLocation,
  location: SourceLocation,
  source
) {
  const func = await findFunctionByName(source, name, index);

  // Fallback onto the location line, if we do not find a function is not found
  let line = location.line;
  if (func) {
    line = func.location.start.line + offset.line;
  }

  return {
    line,
    column: location.column,
    sourceUrl: source.url,
    sourceId: source.id
  };
}

function createSyncData(
  pendingBreakpoint: PendingBreakpoint,
  location: SourceLocation,
  generatedLocation: SourceLocation,
  previousLocation: SourceLocation,
  text: string,
  originalText: string
): BreakpointSyncData {
  const overrides = {
    ...pendingBreakpoint,
    text,
    originalText
  };
  const breakpoint = createBreakpoint(
    { generatedLocation, location },
    overrides
  );

  assertBreakpoint(breakpoint);
  return { breakpoint, previousLocation };
}

// Look for an existing breakpoint at the specified generated location.
function findExistingBreakpoint(state, generatedLocation) {
  const breakpoints = getBreakpointsList(state);

  return breakpoints.find(bp => {
    return (
      bp.generatedLocation.sourceUrl == generatedLocation.sourceUrl &&
      bp.generatedLocation.line == generatedLocation.line &&
      bp.generatedLocation.column == generatedLocation.column
    );
  });
}

// we have three forms of syncing: disabled syncing, existing server syncing
// and adding a new breakpoint
export async function syncBreakpointPromise(
  thunkArgs: ThunkArgs,
  sourceId: SourceId,
  pendingBreakpoint: PendingBreakpoint
): Promise<?BreakpointSyncData> {
  const { getState, client, dispatch } = thunkArgs;
  assertPendingBreakpoint(pendingBreakpoint);

  const source = getSource(getState(), sourceId);

  const generatedSourceId = isOriginalId(sourceId)
    ? originalToGeneratedId(sourceId)
    : sourceId;

  const generatedSource = getSource(getState(), generatedSourceId);

  if (!source || !generatedSource) {
    return;
  }

  const { location, generatedLocation, astLocation } = pendingBreakpoint;
  const previousLocation = { ...location, sourceId };

  const newLocation = await findNewLocation(
    astLocation,
    previousLocation,
    source
  );

  const newGeneratedLocation = await findBreakpointPosition(
    thunkArgs,
    newLocation
  );

  const isSameLocation = comparePosition(
    generatedLocation,
    newGeneratedLocation
  );

  /** ******* CASE 1: No server change ***********/
  // early return if breakpoint is disabled or we are in the sameLocation
  if (newGeneratedLocation && (pendingBreakpoint.disabled || isSameLocation)) {
    // Make sure the breakpoint is installed on all source actors.
    if (!pendingBreakpoint.disabled) {
      await client.setBreakpoint(
        makeBreakpointLocation(getState(), newGeneratedLocation),
        pendingBreakpoint.options
      );
    }

    const originalText = getTextAtPosition(source, previousLocation);
    const text = getTextAtPosition(generatedSource, newGeneratedLocation);

    return createSyncData(
      pendingBreakpoint,
      newLocation,
      newGeneratedLocation,
      previousLocation,
      text,
      originalText
    );
  }

  // Clear any breakpoint for the generated location.
  const bp = findExistingBreakpoint(getState(), generatedLocation);
  if (bp) {
    await dispatch(removeBreakpoint(bp));
  }

  if (!newGeneratedLocation) {
    return { previousLocation, breakpoint: null };
  }

  /** ******* Case 2: Add New Breakpoint ***********/
  // If we are not disabled, set the breakpoint on the server and get
  // that info so we can set it on our breakpoints.
  await client.setBreakpoint(
    makeBreakpointLocation(getState(), newGeneratedLocation),
    pendingBreakpoint.options
  );

  const originalText = getTextAtPosition(source, newLocation);
  const text = getTextAtPosition(generatedSource, newGeneratedLocation);

  return createSyncData(
    pendingBreakpoint,
    newLocation,
    newGeneratedLocation,
    previousLocation,
    text,
    originalText
  );
}

/**
 * Syncing a breakpoint add breakpoint information that is stored, and
 * contact the server for more data.
 */
export function syncBreakpoint(
  sourceId: SourceId,
  pendingBreakpoint: PendingBreakpoint
) {
  return async (thunkArgs: ThunkArgs) => {
    const { dispatch } = thunkArgs;

    const response = await syncBreakpointPromise(
      thunkArgs,
      sourceId,
      pendingBreakpoint
    );

    if (!response) {
      return;
    }

    const { breakpoint, previousLocation } = response;
    return dispatch(
      ({
        type: "SYNC_BREAKPOINT",
        breakpoint,
        previousLocation
      }: Action)
    );
  };
}
