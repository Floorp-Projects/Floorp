/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { setBreakpointPositions } from "./breakpointPositions";
import {
  assertPendingBreakpoint,
  findPosition,
  makeBreakpointLocation,
} from "../../utils/breakpoint";

import { comparePosition, createLocation } from "../../utils/location";

import { originalToGeneratedId, isOriginalId } from "devtools-source-map";
import { getSource } from "../../selectors";
import { addBreakpoint, removeBreakpointAtGeneratedLocation } from ".";

async function findBreakpointPosition(cx, { getState, dispatch }, location) {
  const { sourceId, line } = location;
  const positions = await dispatch(
    setBreakpointPositions({ cx, sourceId, line })
  );

  const position = findPosition(positions, location);
  return position;
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
export function syncBreakpoint(cx, sourceId, pendingBreakpoint) {
  return async thunkArgs => {
    const { getState, client, dispatch } = thunkArgs;
    assertPendingBreakpoint(pendingBreakpoint);

    const source = getSource(getState(), sourceId);

    const generatedSourceId = isOriginalId(sourceId)
      ? originalToGeneratedId(sourceId)
      : sourceId;

    const generatedSource = getSource(getState(), generatedSourceId);

    if (!source || !generatedSource) {
      return null;
    }

    const { location, generatedLocation } = pendingBreakpoint;
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

    const originalLocation = createLocation({
      ...location,
      sourceId,
    });

    const newPosition = await findBreakpointPosition(
      cx,
      thunkArgs,
      originalLocation
    );

    const newGeneratedLocation = newPosition?.generatedLocation;
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
      return null;
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
        newGeneratedLocation,
        pendingBreakpoint.options,
        pendingBreakpoint.disabled
      )
    );
  };
}
