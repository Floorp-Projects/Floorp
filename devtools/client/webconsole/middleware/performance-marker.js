/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { MESSAGES_ADD } = require("devtools/client/webconsole/constants");

const {
  createPerformanceMarkerMiddleware,
} = require("devtools/client/shared/redux/middleware/performance-marker");

module.exports = function(sessionId) {
  return createPerformanceMarkerMiddleware({
    [MESSAGES_ADD]: {
      label: "WebconsoleAddMessages",
      sessionId,
      getMarkerDescription({ action, state }) {
        const { messages } = action;
        const totalMessageCount = state.messages.mutableMessagesById.size;
        return `${messages.length} messages handled, store now has ${totalMessageCount} messages`;
      },
    },
  });
};
