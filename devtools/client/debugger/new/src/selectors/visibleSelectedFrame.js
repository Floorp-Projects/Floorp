/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getSelectedLocation, getSelectedFrame, getCurrentThread } from ".";
import { isOriginalId } from "devtools-source-map";

import type { Frame, SourceLocation } from "../types";
import type { State } from "../reducers/types";

function getLocation(frame: Frame, location: ?SourceLocation) {
  if (!location) {
    return frame.location;
  }

  return !isOriginalId(location.sourceId)
    ? frame.generatedLocation || frame.location
    : frame.location;
}

export function getVisibleSelectedFrame(state: State) {
  const thread = getCurrentThread(state);
  const selectedLocation = getSelectedLocation(state);
  const selectedFrame = getSelectedFrame(state, thread);

  if (!selectedFrame) {
    return null;
  }

  const { id } = selectedFrame;

  return {
    id,
    location: getLocation(selectedFrame, selectedLocation)
  };
}
