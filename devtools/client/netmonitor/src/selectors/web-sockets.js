/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createSelector } = require("devtools/client/shared/vendor/reselect");

/**
 * Returns list of frames that are visible to the user.
 * Filtered frames by types and text are factored in.
 */
const getDisplayedFrames = createSelector(
  state => state.webSockets,
  ({ frames, frameFilterType, frameFilterText, currentChannelId }) => {
    if (!currentChannelId || !frames.get(currentChannelId)) {
      return [];
    }

    const framesArray = frames.get(currentChannelId);
    if (frameFilterType === "all" && frameFilterText.length === 0) {
      return framesArray;
    }

    return framesArray.filter(
      frame =>
        frame.payload.includes(frameFilterText) &&
        (frameFilterType === "all" || frameFilterType === frame.type)
    );
  }
);

/**
 * Checks if the selected frame is visible.
 * If the selected frame is not visible, the SplitBox component
 * should not show the FramePayload component.
 */
const isSelectedFrameVisible = createSelector(
  state => state.webSockets,
  getDisplayedFrames,
  ({ selectedFrame }, displayedFrames) =>
    displayedFrames.some(frame => frame === selectedFrame)
);

/**
 * Returns the current selected frame.
 */
const getSelectedFrame = createSelector(
  state => state.webSockets,
  ({ selectedFrame }) => (selectedFrame ? selectedFrame : undefined)
);

module.exports = {
  getSelectedFrame,
  isSelectedFrameVisible,
  getDisplayedFrames,
};
