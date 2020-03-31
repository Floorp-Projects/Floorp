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
  ({
    frames,
    frameFilterType,
    showControlFrames,
    frameFilterText,
    currentChannelId,
  }) => {
    if (!currentChannelId || !frames.get(currentChannelId)) {
      return [];
    }

    const framesArray = frames.get(currentChannelId);
    if (frameFilterType === "all" && frameFilterText.length === 0) {
      return framesArray.filter(frame =>
        typeFilter(frame, frameFilterType, showControlFrames)
      );
    }

    const filter = searchFilter(frameFilterText);

    // If frame payload is > 10,000 characters long, we check the LongStringActor payload string
    return framesArray.filter(
      frame =>
        (frame.payload.initial
          ? filter(frame.payload.initial)
          : filter(frame.payload)) &&
        typeFilter(frame, frameFilterType, showControlFrames)
    );
  }
);

function typeFilter(frame, frameFilterType, showControlFrames) {
  const controlFrames = [0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf];
  const isControlFrame = controlFrames.includes(frame.opCode);
  if (frameFilterType === "all" || frameFilterType === frame.type) {
    return showControlFrames || !isControlFrame;
  }
  return false;
}

function searchFilter(frameFilterText) {
  let regex;
  if (looksLikeRegex(frameFilterText)) {
    try {
      regex = regexFromText(frameFilterText);
    } catch (e) {}
  }

  return regex
    ? payload => regex.test(payload)
    : payload => payload.includes(frameFilterText);
}

function looksLikeRegex(text) {
  return text.startsWith("/") && text.endsWith("/") && text.length > 2;
}

function regexFromText(text) {
  return new RegExp(text.slice(1, -1), "im");
}

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

/**
 * Returns summary data of the list of frames that are visible to the user.
 * Filtered frames by types and text are factored in.
 */
const getDisplayedFramesSummary = createSelector(
  getDisplayedFrames,
  displayedFrames => {
    let firstStartedMs = +Infinity;
    let lastEndedMs = -Infinity;
    let sentSize = 0;
    let receivedSize = 0;
    let totalSize = 0;

    displayedFrames.forEach(frame => {
      if (frame.type == "received") {
        receivedSize += frame.payload.length;
      } else if (frame.type == "sent") {
        sentSize += frame.payload.length;
      }
      totalSize += frame.payload.length;
      if (frame.timeStamp < firstStartedMs) {
        firstStartedMs = frame.timeStamp;
      }
      if (frame.timeStamp > lastEndedMs) {
        lastEndedMs = frame.timeStamp;
      }
    });

    return {
      count: displayedFrames.length,
      totalMs: (lastEndedMs - firstStartedMs) / 1000,
      sentSize,
      receivedSize,
      totalSize,
    };
  }
);

/**
 * Returns if the currentChannelId is closed
 */
const isCurrentChannelClosed = createSelector(
  state => state.webSockets,
  ({ closedConnections, currentChannelId }) =>
    closedConnections.has(currentChannelId)
);

/**
 * Returns the closed connection details of the currentChannelId
 * Null, if the connection is still open
 */
const getClosedConnectionDetails = createSelector(
  state => state.webSockets,
  ({ closedConnections, currentChannelId }) =>
    closedConnections.get(currentChannelId)
);

module.exports = {
  getSelectedFrame,
  isSelectedFrameVisible,
  getDisplayedFrames,
  getDisplayedFramesSummary,
  isCurrentChannelClosed,
  getClosedConnectionDetails,
};
