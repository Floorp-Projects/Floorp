/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { MESSAGES_ADD } = require("devtools/client/webconsole/constants");

const ChromeUtils = require("ChromeUtils");
const { Cu } = require("chrome");

/**
 * Performance marker middleware is responsible for recording markers visible
 * performance profiles.
 */
function performanceMarkerMiddleware(store) {
  return next => action => {
    const shouldAddProfileMarker = action.type === MESSAGES_ADD;

    // Start the marker timer before calling next(action).
    const startTime = shouldAddProfileMarker ? Cu.now() : null;
    const state = next(action);

    if (shouldAddProfileMarker) {
      const { messages } = action;
      const totalMessageCount = store.getState().messages.messagesById.size;
      ChromeUtils.addProfilerMarker(
        "WebconsoleAddMessages",
        startTime,
        `${messages.length} messages handled, store now has ${totalMessageCount} messages`
      );
    }
    return state;
  };
}

module.exports = performanceMarkerMiddleware;
