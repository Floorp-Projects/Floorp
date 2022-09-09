/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");

/**
 * This function returns a middleware, which is responsible for adding markers that will
 * be visible in performance profiles, and may help investigate performance issues.
 *
 * Example usage, adding a marker when console messages are added, and when they are cleared:
 *
 * return createPerformanceMarkerMiddleware({
 *   "MESSAGES_ADD": {
 *     label: "WebconsoleAddMessages",
 *     sessionId: 12345,
 *     getMarkerDescription: function({ action, state }) {
 *       const { messages } = action;
 *       const totalMessageCount = state.messages.mutableMessagesById.size;
 *       return `${messages.length} messages handled, store now has ${totalMessageCount} messages`;
 *     },
 *   },
 *   "MESSAGES_CLEARED": {
 *     label: "WebconsoleClearMessages",
 *     sessionId: 12345
 *   },
 * });
 *
 * @param {Object} cases: An object, keyed by action type, that will determine if a
 *         given action will add a marker.
 * @param {String} cases.{actionType} - The type of the action that will trigger the
 *         marker creation.
 * @param {String} cases.{actionType}.label - The marker label
 * @param {Integer} cases.{actionType}.sessionId - The telemetry sessionId. This is used
 *        to be able to distinguish markers coming from different toolboxes.
 * @param {Function} [cases.{actionType}.getMarkerDescription] - An optional function that
 *        will be called when adding the marker to populate its description. The function
 *        is called with an object holding the action and the state
 */
function createPerformanceMarkerMiddleware(cases) {
  return function(store) {
    return next => action => {
      const condition = cases[action.type];
      const shouldAddProfileMarker = !!condition;

      // Start the marker timer before calling next(action).
      const startTime = shouldAddProfileMarker ? Cu.now() : null;
      const newState = next(action);

      if (shouldAddProfileMarker) {
        ChromeUtils.addProfilerMarker(
          `${condition.label} ${condition.sessionId}`,
          startTime,
          condition.getMarkerDescription
            ? condition.getMarkerDescription({
                action,
                state: store.getState(),
              })
            : ""
        );
      }
      return newState;
    };
  };
}

module.exports = {
  createPerformanceMarkerMiddleware,
};
