/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createSelector } = require("devtools/client/shared/vendor/reselect");

function getFramesByChannelId(state, channelId) {
  return state.webSockets.frames.get(channelId);
}

/**
 * Checks if the selected frame is visible.
 * If the selected frame is not visible, the SplitBox component
 * should not show the FramePayload component.
 */
function isSelectedFrameVisible(state, channelId, targetFrame) {
  const displayedFrames = getFramesByChannelId(state, channelId);
  if (displayedFrames && targetFrame) {
    return displayedFrames.some(frame => frame === targetFrame);
  }
  return false;
}

const getSelectedFrame = createSelector(
  state => state.webSockets,
  ({ selectedFrame }) => (selectedFrame ? selectedFrame : undefined)
);

module.exports = {
  getFramesByChannelId,
  getSelectedFrame,
  isSelectedFrameVisible,
};
