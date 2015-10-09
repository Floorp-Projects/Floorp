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
   * @param {Object} options Options for the store. Should contain the
   *                         activeRoomStore, dispatcher and mozLoop objects.
   */
  var ConversationAppStore = function(options) {
    if (!options.activeRoomStore) {
      throw new Error("Missing option activeRoomStore");
    }
    if (!options.dispatcher) {
      throw new Error("Missing option dispatcher");
    }
    if (!options.mozLoop) {
      throw new Error("Missing option mozLoop");
    }

    this._activeRoomStore = options.activeRoomStore;
    this._dispatcher = options.dispatcher;
    this._mozLoop = options.mozLoop;
    this._rootObj = ("rootObject" in options) ? options.rootObject : window;
    this._storeState = this.getInitialStoreState();

    // Start listening for specific events, coming from the window object.
    this._eventHandlers = {};
    ["unload", "LoopHangupNow", "socialFrameAttached", "socialFrameDetached"]
      .forEach(function(eventName) {
        var handlerName = eventName + "Handler";
        this._eventHandlers[eventName] = this[handlerName].bind(this);
        this._rootObj.addEventListener(eventName, this._eventHandlers[eventName]);
      }.bind(this));

    this._dispatcher.register(this, [
      "getWindowData",
      "showFeedbackForm"
    ]);
  };

  ConversationAppStore.prototype = _.extend({
    getInitialStoreState: function() {
      return {
        chatWindowDetached: false,
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
      this._storeState = _.extend({}, this._storeState, state);
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
    },

    /**
     * Event handler; invoked when the 'unload' event is dispatched from the
     * window object.
     * It will dispatch a 'WindowUnload' action that other stores may listen to
     * and will remove all event handlers attached to the window object.
     */
    unloadHandler: function() {
      this._dispatcher.dispatch(new loop.shared.actions.WindowUnload());

      // Unregister event handlers.
      var eventNames = Object.getOwnPropertyNames(this._eventHandlers);
      eventNames.forEach(function(eventName) {
        this._rootObj.removeEventListener(eventName, this._eventHandlers[eventName]);
      }.bind(this));
      this._eventHandlers = null;
    },

    /**
     * Event handler; invoked when the 'LoopHangupNow' event is dispatched from
     * the window object.
     * It'll attempt to gracefully disconnect from an active session, or close
     * the window when no session is currently active.
     */
    LoopHangupNowHandler: function() {
      switch (this.getStoreState().windowType) {
        case "room":
          if (this._activeRoomStore.getStoreState().used &&
              !this._storeState.showFeedbackForm) {
            this._dispatcher.dispatch(new loop.shared.actions.LeaveRoom());
          } else {
            loop.shared.mixins.WindowCloseMixin.closeWindow();
          }
          break;
        default:
          loop.shared.mixins.WindowCloseMixin.closeWindow();
          break;
      }
    },

    /**
     * Event handler; invoked when the 'socialFrameAttached' event is dispatched
     * from the window object.
     */
    socialFrameAttachedHandler: function() {
      this.setStoreState({ chatWindowDetached: false });
    },

    /**
     * Event handler; invoked when the 'socialFrameDetached' event is dispatched
     * from the window object.
     */
    socialFrameDetachedHandler: function() {
      this.setStoreState({ chatWindowDetached: true });
    }
  }, Backbone.Events);

  return ConversationAppStore;

})();
