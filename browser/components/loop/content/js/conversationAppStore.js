/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.store = loop.store || {};

/**
 * Manages the conversation window app controller view. Used to get
 * the window data and store the window type.
 */
loop.store.ConversationAppStore = (function() {
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
    this._storeState = {};

    this._dispatcher.register(this, [
      "getWindowData"
    ]);
  };

  ConversationAppStore.prototype = _.extend({
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
     * Handles the get window data action - obtains the window data,
     * updates the store and notifies interested components.
     *
     * @param {sharedActions.GetWindowData} actionData The action data
     */
    getWindowData: function(actionData) {
      var windowData;
      // XXX Remove me in bug 1074678
      if (this._mozLoop.getLoopBoolPref("test.alwaysUseRooms")) {
        windowData = {type: "room", localRoomId: "42"};
      } else {
        windowData = this._mozLoop.getConversationWindowData(actionData.windowId);
      }

      if (!windowData) {
        console.error("Failed to get the window data");
        this.setStoreState({windowType: "failed"});
        return;
      }

      // XXX windowData is a hack for the IncomingConversationView until
      // we rework it for the flux model in bug 1088672.
      this.setStoreState({
        windowType: windowData.type,
        windowData: windowData
      });

      this._dispatcher.dispatch(new loop.shared.actions.SetupWindowData(_.extend({
        windowId: actionData.windowId}, windowData)));
    }
  }, Backbone.Events);

  return ConversationAppStore;

})();
