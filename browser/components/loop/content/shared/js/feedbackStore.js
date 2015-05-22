/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

loop.store.FeedbackStore = (function() {
  "use strict";

  var sharedActions = loop.shared.actions;
  var FEEDBACK_STATES = loop.store.FEEDBACK_STATES = {
    // Initial state (mood selection)
    INIT: "feedback-init",
    // User detailed feedback form step
    DETAILS: "feedback-details",
    // Pending feedback data submission
    PENDING: "feedback-pending",
    // Feedback has been sent
    SENT: "feedback-sent",
    // There was an issue with the feedback API
    FAILED: "feedback-failed"
  };

  /**
   * Feedback store.
   *
   * @param {loop.Dispatcher} dispatcher  The dispatcher for dispatching actions
   *                                      and registering to consume actions.
   * @param {Object} options Options object:
   * - {mozLoop}        mozLoop                 The MozLoop API object.
   * - {feedbackClient} loop.FeedbackAPIClient  The feedback API client.
   */
  var FeedbackStore = loop.store.createStore({
    actions: [
      "requireFeedbackDetails",
      "sendFeedback",
      "sendFeedbackError",
      "feedbackComplete"
    ],

    initialize: function(options) {
      if (!options.feedbackClient) {
        throw new Error("Missing option feedbackClient");
      }
      this._feedbackClient = options.feedbackClient;
    },

    /**
     * Returns initial state data for this active room.
     */
    getInitialStoreState: function() {
      return {feedbackState: FEEDBACK_STATES.INIT};
    },

    /**
     * Requires user detailed feedback.
     */
    requireFeedbackDetails: function() {
      this.setStoreState({feedbackState: FEEDBACK_STATES.DETAILS});
    },

    /**
     * Sends feedback data to the feedback server.
     *
     * @param {sharedActions.SendFeedback} actionData The action data.
     */
    sendFeedback: function(actionData) {
      delete actionData.name;
      this._feedbackClient.send(actionData, function(err) {
        if (err) {
          this.dispatchAction(new sharedActions.SendFeedbackError({
            error: err
          }));
          return;
        }
        this.setStoreState({feedbackState: FEEDBACK_STATES.SENT});
      }.bind(this));

      this.setStoreState({feedbackState: FEEDBACK_STATES.PENDING});
    },

    /**
     * Notifies a store from any error encountered while sending feedback data.
     *
     * @param {sharedActions.SendFeedback} actionData The action data.
     */
    sendFeedbackError: function(actionData) {
      this.setStoreState({
        feedbackState: FEEDBACK_STATES.FAILED,
        error: actionData.error
      });
    },

    /**
     * Resets the store to its initial state as feedback has been completed,
     * i.e. ready for the next round of feedback.
     */
    feedbackComplete: function() {
      this.resetStoreState();
    }
  });

  return FeedbackStore;
})();
