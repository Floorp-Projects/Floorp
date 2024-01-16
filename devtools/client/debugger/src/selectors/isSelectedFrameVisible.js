/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { originalToGeneratedId } from "devtools/client/shared/source-map-loader/index";
import { getSelectedFrame, getSelectedLocation, getCurrentThread } from ".";

function getGeneratedId(source) {
  if (source.isOriginal) {
    return originalToGeneratedId(source.id);
  }

  return source.id;
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

  if (selectedLocation.source.isOriginal) {
    return selectedLocation.source.id === selectedFrame.location.source.id;
  }

  return (
    selectedLocation.source.id === getGeneratedId(selectedFrame.location.source)
  );
}
