/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

/**
 * Manages the conversation window app controller view. Used to get
 * the window data and store the window type.
 */
loop.store.ConversationAppStore = (function() {
  "use strict";

  /**
   * Constructor
   *
   * @param {Object} options Options for the store. Should contain the dispatcher.
   */
  var ConversationAppStore = function(options) {
    if (!options.dispatcher) {
      throw new Error("Missing option dispatcher");
    }
    if (!options.mozLoop) {
      throw new Error("Missing option mozLoop");
    }

    this._dispatcher = options.dispatcher;
    this._mozLoop = options.mozLoop;
    this._storeState = this.getInitialStoreState();

    this._dispatcher.register(this, [
      "getWindowData",
      "showFeedbackForm"
    ]);
  };

  ConversationAppStore.prototype = _.extend({
    getInitialStoreState: function() {
      return {
        // How often to display the form. Convert seconds to ms.
        feedbackPeriod: this._mozLoop.getLoopPref("feedback.periodSec") * 1000,
        // Date when the feedback form was last presented. Convert to ms.
        feedbackTimestamp: this._mozLoop
                               .getLoopPref("feedback.dateLastSeenSec") * 1000,
        showFeedbackForm: false
      };
    },

    /**
     * Retrieves current store state.
     *
     * @return {Object}
     */
    getStoreState: function() {
      return this._storeState;
    },

    /**
     * Updates store states and trigger a "change" event.
     *
     * @param {Object} state The new store state.
     */
    setStoreState: function(state) {
      this._storeState = state;
      this.trigger("change");
    },

    /**
     * Sets store state which will result in the feedback form rendered.
     * Saves a timestamp of when the feedback was last rendered.
     */
    showFeedbackForm: function() {
      var timestamp = Math.floor(new Date().getTime() / 1000);

      this._mozLoop.setLoopPref("feedback.dateLastSeenSec", timestamp);

      this.setStoreState({
        showFeedbackForm: true
      });
    },

    /**
     * Handles the get window data action - obtains the window data,
     * updates the store and notifies interested components.
     *
     * @param {sharedActions.GetWindowData} actionData The action data
     */
    getWindowData: function(actionData) {
      var windowData = this._mozLoop.getConversationWindowData(actionData.windowId);

      if (!windowData) {
        console.error("Failed to get the window data");
        this.setStoreState({windowType: "failed"});
        return;
      }

      this.setStoreState({windowType: windowData.type});

      this._dispatcher.dispatch(new loop.shared.actions.SetupWindowData(_.extend({
        windowId: actionData.windowId}, windowData)));
    }
  }, Backbone.Events);

  return ConversationAppStore;

})();
