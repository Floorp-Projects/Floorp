/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

/**
 * Manages the standalone app controller view. Used to get
 * the window data and store the window type.
 */
loop.store.StandaloneAppStore = (function() {
  "use strict";

  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;

  var CALL_REGEXP = /\/c\/([\w\-]+)$/;
  var ROOM_REGEXP = /\/([\w\-]+)$/;

  /**
   * Constructor
   *
   * @param {Object} options Options for the store. Should contain the
   *                         dispatcher.
   */
  var StandaloneAppStore = function(options) {
    if (!options.dispatcher) {
      throw new Error("Missing option dispatcher");
    }
    if (!options.sdk) {
      throw new Error("Missing option sdk");
    }

    this._dispatcher = options.dispatcher;
    this._storeState = {};
    this._sdk = options.sdk;

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
        var pathMatch = path.match(regexp);
        if (pathMatch && pathMatch[1]) {
          return pathMatch;
        }
        return null;
      }

      if (windowPath) {
        match = extractId(windowPath, CALL_REGEXP);

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
     * Extracts the crypto key from the hash for the page.
     */
    _extractCryptoKey: function(windowHash) {
      if (windowHash && windowHash[0] === "#") {
        return windowHash.substring(1, windowHash.length);
      }

      return null;
    },

    /**
     * Handles the extract token info action - obtains the token information
     * and its type; extracts any crypto information; updates the store and
     * notifies interested components.
     *
     * @param {sharedActions.GetWindowData} actionData The action data
     */
    extractTokenInfo: function(actionData) {
      var windowType = "home";
      var token;

      // Check if we're on a supported device/platform.
      var unsupportedPlatform =
        sharedUtils.getUnsupportedPlatform(navigator.platform);

      if (unsupportedPlatform) {
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

      this.setStoreState({
        windowType: windowType,
        isFirefox: sharedUtils.isFirefox(navigator.userAgent),
        unsupportedPlatform: unsupportedPlatform
      });

      // If we've not got a window ID, don't dispatch the action, as we don't
      // need it.
      if (token) {
        this._dispatcher.dispatch(new loop.shared.actions.FetchServerData({
          cryptoKey: this._extractCryptoKey(actionData.windowHash),
          token: token,
          windowType: windowType
        }));
      }
    }
  }, Backbone.Events);

  return StandaloneAppStore;
})();
