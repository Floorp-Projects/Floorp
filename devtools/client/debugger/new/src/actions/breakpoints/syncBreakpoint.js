/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { setBreakpointPositions } from "./breakpointPositions";
import {
  locationMoved,
  createBreakpoint,
  assertBreakpoint,
  assertPendingBreakpoint,
  findScopeByName,
  makeBreakpointLocation
} from "../../utils/breakpoint";

import { getGeneratedLocation } from "../../utils/source-maps";
import { getTextAtPosition } from "../../utils/source";
import { originalToGeneratedId, isOriginalId } from "devtools-source-map";
import { getSource } from "../../selectors";
import { features } from "../../utils/prefs";

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

async function makeScopedLocation(
  { name, offset, index }: ASTLocation,
  location: SourceLocation,
  source
) {
  const scope = await findScopeByName(source, name, index);
  // fallback onto the location line, if the scope is not found
  // note: we may at some point want to delete the breakpoint if the scope
  // disappears
  const line = scope ? scope.location.start.line + offset.line : location.line;
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
    generatedLocation,
    text,
    originalText
  };
  const breakpoint = createBreakpoint(location, overrides);

  assertBreakpoint(breakpoint);
  return { breakpoint, previousLocation };
}

// we have three forms of syncing: disabled syncing, existing server syncing
// and adding a new breakpoint
export async function syncBreakpointPromise(
  getState: Function,
  client: Object,
  sourceMaps: Object,
  dispatch: Function,
  sourceId: SourceId,
  pendingBreakpoint: PendingBreakpoint
): Promise<BreakpointSyncData | null> {
  assertPendingBreakpoint(pendingBreakpoint);

  const source = getSource(getState(), sourceId);

  const generatedSourceId = isOriginalId(sourceId)
    ? originalToGeneratedId(sourceId)
    : sourceId;

  const generatedSource = getSource(getState(), generatedSourceId);

  if (!source) {
    return null;
  }

  const { location, astLocation } = pendingBreakpoint;
  const previousLocation = { ...location, sourceId };

  const scopedLocation = await makeScopedLocation(
    astLocation,
    previousLocation,
    source
  );

  const scopedGeneratedLocation = await getGeneratedLocation(
    getState(),
    source,
    scopedLocation,
    sourceMaps
  );

  // this is the generatedLocation of the pending breakpoint, with
  // the source id updated to reflect the new connection
  const generatedLocation = {
    ...pendingBreakpoint.generatedLocation,
    sourceId: generatedSourceId
  };

  const isSameLocation = !locationMoved(
    generatedLocation,
    scopedGeneratedLocation
  );

  // makeBreakpointLocation requires the source to still exist, which might not
  // be the case if we navigated.
  if (!getSource(getState(), generatedSourceId)) {
    return null;
  }

  const breakpointLocation = makeBreakpointLocation(
    getState(),
    generatedLocation
  );

  let possiblePosition = true;
  if (features.columnBreakpoints && generatedLocation.column != undefined) {
    const { positions } = await dispatch(
      setBreakpointPositions(generatedLocation)
    );
    if (!positions.includes(generatedLocation.column)) {
      possiblePosition = false;
    }
  }

  /** ******* CASE 1: No server change ***********/
  // early return if breakpoint is disabled or we are in the sameLocation
  if (possiblePosition && (pendingBreakpoint.disabled || isSameLocation)) {
    // Make sure the breakpoint is installed on all source actors.
    if (!pendingBreakpoint.disabled) {
      await client.setBreakpoint(breakpointLocation, pendingBreakpoint.options);
    }

    const originalText = getTextAtPosition(source, previousLocation);
    const text = getTextAtPosition(generatedSource, generatedLocation);

    return createSyncData(
      pendingBreakpoint,
      scopedLocation,
      scopedGeneratedLocation,
      previousLocation,
      text,
      originalText
    );
  }

  // clear server breakpoints if they exist and we have moved
  await client.removeBreakpoint(breakpointLocation);

  if (!possiblePosition || !scopedGeneratedLocation.line) {
    return { previousLocation, breakpoint: null };
  }

  /** ******* Case 2: Add New Breakpoint ***********/
  // If we are not disabled, set the breakpoint on the server and get
  // that info so we can set it on our breakpoints.
  await client.setBreakpoint(
    scopedGeneratedLocation,
    pendingBreakpoint.options
  );

  const originalText = getTextAtPosition(source, scopedLocation);
  const text = getTextAtPosition(generatedSource, scopedGeneratedLocation);

  return createSyncData(
    pendingBreakpoint,
    scopedLocation,
    scopedGeneratedLocation,
    previousLocation,
    text,
    originalText
  );
}

/**
 * Syncing a breakpoint add breakpoint information that is stored, and
 * contact the server for more data.
 *
 * @memberof actions/breakpoints
 * @static
 * @param {String} $1.sourceId String  value
 * @param {PendingBreakpoint} $1.location PendingBreakpoint  value
 */
export function syncBreakpoint(
  sourceId: SourceId,
  pendingBreakpoint: PendingBreakpoint
) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const response = await syncBreakpointPromise(
      getState,
      client,
      sourceMaps,
      dispatch,
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
