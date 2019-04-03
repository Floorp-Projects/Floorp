/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { setBreakpointPositions } from "./breakpointPositions";
import {
  assertPendingBreakpoint,
  findFunctionByName,
  findPosition,
  makeBreakpointLocation
} from "../../utils/breakpoint";

import { comparePosition, createLocation } from "../../utils/location";

import { originalToGeneratedId, isOriginalId } from "devtools-source-map";
import { getSource, getBreakpoint } from "../../selectors";
import { removeBreakpoint, addBreakpoint } from ".";

import type { ThunkArgs } from "../types";

import type {
  SourceLocation,
  ASTLocation,
  PendingBreakpoint,
  SourceId
} from "../../types";

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
      sourceId: generatedSourceId
    });

    if (
      source == generatedSource &&
      location.sourceUrl != generatedLocation.sourceUrl
    ) {
      // We are handling the generated source and the pending breakpoint has a
      // source mapping. Watch out for the case when the original source has
      // already been processed, in which case either a breakpoint has already
      // been added at this generated location or the client breakpoint has been
      // removed.
      const breakpointLocation = makeBreakpointLocation(
        getState(),
        sourceGeneratedLocation
      );
      if (
        getBreakpoint(getState(), sourceGeneratedLocation) ||
        !client.hasBreakpoint(breakpointLocation)
      ) {
        return;
      }
      return dispatch(
        addBreakpoint(
          sourceGeneratedLocation,
          pendingBreakpoint.options,
          pendingBreakpoint.disabled
        )
      );
    }

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

    if (!newGeneratedLocation) {
      return;
    }

    const isSameLocation = comparePosition(
      generatedLocation,
      newGeneratedLocation
    );

    // If the new generated location has changed from that in the pending
    // breakpoint, remove any breakpoint associated with the old generated
    // location. This could either be in the reducer or only in the client,
    // depending on whether the pending breakpoint has been processed for the
    // generated source yet.
    if (!isSameLocation) {
      const bp = getBreakpoint(getState(), sourceGeneratedLocation);
      if (bp) {
        dispatch(removeBreakpoint(bp));
      } else {
        const breakpointLocation = makeBreakpointLocation(
          getState(),
          sourceGeneratedLocation
        );
        client.removeBreakpoint(breakpointLocation);
      }
    }

    return dispatch(
      addBreakpoint(
        newLocation,
        pendingBreakpoint.options,
        pendingBreakpoint.disabled
      )
    );
  };
}
