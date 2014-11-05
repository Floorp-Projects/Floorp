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
loop.store.StandaloneAppStore = (function() {
  "use strict";

  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;

  var OLD_STYLE_CALL_REGEXP = /\#call\/(.*)/;
  var NEW_STYLE_CALL_REGEXP = /\/c\/([\w\-]+)$/;
  var ROOM_REGEXP = /\/([\w\-]+)$/;

  /**
   * Constructor
   *
   * @param {Object} options Options for the store. Should contain the dispatcher.
   */
  var StandaloneAppStore = function(options) {
    if (!options.dispatcher) {
      throw new Error("Missing option dispatcher");
    }
    if (!options.sdk) {
      throw new Error("Missing option sdk");
    }
    if (!options.helper) {
      throw new Error("Missing option helper");
    }
    if (!options.conversation) {
      throw new Error("Missing option conversation");
    }

    this._dispatcher = options.dispatcher;
    this._storeState = {};
    this._sdk = options.sdk;
    this._helper = options.helper;
    this._conversation = options.conversation;

    this._dispatcher.register(this, [
      "extractTokenInfo"
    ]);
  };

  StandaloneAppStore.prototype = _.extend({
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

    _extractWindowDataFromPath: function(windowPath) {
      var match;
      var windowType = "home";

      function extractId(path, regexp) {
        var match = path.match(regexp);
        if (match && match[1]) {
          return match;
        }
        return null;
      }

      if (windowPath) {
        // Is this a call url (the hash is a backwards-compatible url)?
        match = extractId(windowPath, OLD_STYLE_CALL_REGEXP) ||
                extractId(windowPath, NEW_STYLE_CALL_REGEXP);

        if (match) {
          windowType = "outgoing";
        } else {
          // Is this a room url?
          match = extractId(windowPath, ROOM_REGEXP);

          if (match) {
            windowType = "room";
          }
        }
      }
      return [windowType, match && match[1] ? match[1] : null];
    },

    /**
     * Handles the extract token info action - obtains the token information
     * and its type; updates the store and notifies interested components.
     *
     * @param {sharedActions.GetWindowData} actionData The action data
     */
    extractTokenInfo: function(actionData) {
      var windowType = "home";
      var token;

      // Check if we're on a supported device/platform.
      if (this._helper.isIOS(navigator.platform)) {
        windowType = "unsupportedDevice";
      } else if (!this._sdk.checkSystemRequirements()) {
        windowType = "unsupportedBrowser";
      } else if (actionData.windowPath) {
        // ES6 not used in standalone yet.
        var result = this._extractWindowDataFromPath(actionData.windowPath);
        windowType = result[0];
        token = result[1];
      }
      // Else type is home.

      if (token) {
        this._conversation.set({loopToken: token});
      }

      this.setStoreState({
        windowType: windowType
      });

      // If we've not got a window ID, don't dispatch the action, as we don't need
      // it.
      if (token) {
        this._dispatcher.dispatch(new loop.shared.actions.FetchServerData({
          token: token,
          windowType: windowType
        }));
      }
    }
  }, Backbone.Events);

  return StandaloneAppStore;
})();
