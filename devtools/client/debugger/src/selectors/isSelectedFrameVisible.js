/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  originalToGeneratedId,
  isOriginalId,
} from "devtools/client/shared/source-map-loader/index";
import { getSelectedFrame, getSelectedLocation, getCurrentThread } from ".";

function getGeneratedId(sourceId) {
  if (isOriginalId(sourceId)) {
    return originalToGeneratedId(sourceId);
  }

  return sourceId;
}

/*
 * Checks to if the selected frame's source is currently
 * selected.
 */
export function isSelectedFrameVisible(state) {
  const thread = getCurrentThread(state);
  const selectedLocation = getSelectedLocation(state);
  const selectedFrame = getSelectedFrame(state, thread);

  if (!selectedFrame || !selectedLocation) {
    return false;
  }

  if (isOriginalId(selectedLocation.source.id)) {
    return selectedLocation.source.id === selectedFrame.location.source.id;
  }

  return (
    selectedLocation.source.id ===
    getGeneratedId(selectedFrame.location.source.id)
  );
}
