/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { isEqual } from "lodash";
import { isGeneratedId } from "devtools-source-map";
import type { Frame, MappedLocation } from "../../types";
import type { State } from "../../reducers/types";
import { isOriginal } from "../../utils/source";

import {
  getSelectedSource,
  getPreviousPauseFrameLocation,
  getPausePoint
} from "../../selectors";

function getFrameLocation(source, frame: ?MappedLocation) {
  if (!frame) {
    return null;
  }

  return isOriginal(source) ? frame.location : frame.generatedLocation;
}

export function shouldStep(rootFrame: ?Frame, state: State, sourceMaps: any) {
  const selectedSource = getSelectedSource(state);
  const previousFrameInfo = getPreviousPauseFrameLocation(state);

  if (!rootFrame || !selectedSource) {
    return false;
  }

  const previousFrameLoc = getFrameLocation(selectedSource, previousFrameInfo);
  const frameLoc = getFrameLocation(selectedSource, rootFrame);

  const sameLocation = previousFrameLoc && isEqual(previousFrameLoc, frameLoc);
  const pausePoint = getPausePoint(state, frameLoc);
  const invalidPauseLocation = pausePoint && !pausePoint.step;

  // We always want to pause in generated locations
  if (!frameLoc || isGeneratedId(frameLoc.sourceId)) {
    return false;
  }

  return sameLocation || invalidPauseLocation;
}
