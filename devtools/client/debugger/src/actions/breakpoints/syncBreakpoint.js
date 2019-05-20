/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { setBreakpointPositions } from "./breakpointPositions";
import { setSymbols } from "../sources/symbols";
import {
  assertPendingBreakpoint,
  findFunctionByName,
  findPosition,
  makeBreakpointLocation,
} from "../../utils/breakpoint";

import { comparePosition, createLocation } from "../../utils/location";

import { originalToGeneratedId, isOriginalId } from "devtools-source-map";
import { getSource } from "../../selectors";
import { addBreakpoint, removeBreakpointAtGeneratedLocation } from ".";

import type { ThunkArgs } from "../types";
import type { LoadedSymbols } from "../../reducers/types";

import type {
  SourceLocation,
  ASTLocation,
  PendingBreakpoint,
  SourceId,
  BreakpointPositions,
  Context,
} from "../../types";

async function findBreakpointPosition(
  cx: Context,
  { getState, dispatch },
  location: SourceLocation
) {
  const { sourceId, line } = location;
  const positions: BreakpointPositions = await dispatch(
    setBreakpointPositions({ cx, sourceId, line })
  );

  const position = findPosition(positions, location);
  return position && position.generatedLocation;
}

async function findNewLocation(
  cx: Context,
  { name, offset, index }: ASTLocation,
  location: SourceLocation,
  source,
  thunkArgs
) {
  const symbols: LoadedSymbols = await thunkArgs.dispatch(
    setSymbols({ cx, source })
  );
  const func = symbols ? findFunctionByName(symbols, name, index) : null;

  // Fallback onto the location line, if we do not find a function.
  let line = location.line;
  if (func) {
    line = func.location.start.line + offset.line;
  }

  return {
    line,
    column: location.column,
    sourceUrl: source.url,
    sourceId: source.id,
  };
}

// Breakpoint syncing occurs when a source is found that matches either the
// original or generated URL of a pending breakpoint. A new breakpoint is
// constructed that might have a different original and/or generated location,
// if the original source has changed since the pending breakpoint was created.
// There are a couple subtle aspects to syncing:
//
// - We handle both the original and generated source because there is no
//   guarantee that seeing the generated source means we will also see the
//   original source. When connecting, a breakpoint will be installed in the
//   client for the generated location in the pending breakpoint, and we need
//   to make sure that either a breakpoint is added to the reducer or that this
//   client breakpoint is deleted.
//
// - If we see both the original and generated sources and the source mapping
//   has changed, we need to make sure that only a single breakpoint is added
//   to the reducer for the new location corresponding to the original location
//   in the pending breakpoint.
export function syncBreakpoint(
  cx: Context,
  sourceId: SourceId,
  pendingBreakpoint: PendingBreakpoint
) {
  return async (thunkArgs: ThunkArgs) => {
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
    const sourceGeneratedLocation = createLocation({
      ...generatedLocation,
      sourceId: generatedSourceId,
    });

    if (
      source == generatedSource &&
      location.sourceUrl != generatedLocation.sourceUrl
    ) {
      // We are handling the generated source and the pending breakpoint has a
      // source mapping. Supply a cancellation callback that will abort the
      // breakpoint if the original source was synced to a different location,
      // in which case the client breakpoint has been removed.
      const breakpointLocation = makeBreakpointLocation(
        getState(),
        sourceGeneratedLocation
      );
      return dispatch(
        addBreakpoint(
          cx,
          sourceGeneratedLocation,
          pendingBreakpoint.options,
          pendingBreakpoint.disabled,
          () => !client.hasBreakpoint(breakpointLocation)
        )
      );
    }

    const previousLocation = { ...location, sourceId };

    const newLocation = await findNewLocation(
      cx,
      astLocation,
      previousLocation,
      source,
      thunkArgs
    );

    const newGeneratedLocation = await findBreakpointPosition(
      cx,
      thunkArgs,
      newLocation
    );

    if (!newGeneratedLocation) {
      // We couldn't find a new mapping for the breakpoint. If there is a source
      // mapping, remove any breakpoints for the generated location, as if the
      // breakpoint moved. If the old generated location still maps to an
      // original location then we don't want to add a breakpoint for it.
      if (location.sourceUrl != generatedLocation.sourceUrl) {
        dispatch(
          removeBreakpointAtGeneratedLocation(cx, sourceGeneratedLocation)
        );
      }
      return;
    }

    const isSameLocation = comparePosition(
      generatedLocation,
      newGeneratedLocation
    );

    // If the new generated location has changed from that in the pending
    // breakpoint, remove any breakpoint associated with the old generated
    // location.
    if (!isSameLocation) {
      dispatch(
        removeBreakpointAtGeneratedLocation(cx, sourceGeneratedLocation)
      );
    }

    return dispatch(
      addBreakpoint(
        cx,
        newLocation,
        pendingBreakpoint.options,
        pendingBreakpoint.disabled
      )
    );
  };
}
