/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.store = loop.store || {};

loop.store.FxOSActiveRoomStore = (function() {
  "use strict";
  var sharedActions = loop.shared.actions;
  var ROOM_STATES = loop.store.ROOM_STATES;

  var FxOSActiveRoomStore = loop.store.createStore({
    actions: [
      "fetchServerData"
    ],

    initialize: function(options) {
      if (!options.mozLoop) {
        throw new Error("Missing option mozLoop");
      }
      this._mozLoop = options.mozLoop;
    },

    /**
     * Returns initial state data for this active room.
     */
    getInitialStoreState: function() {
      return {
        roomState: ROOM_STATES.INIT,
        audioMuted: false,
        videoMuted: false,
        failureReason: undefined,
        localVideoDimensions: {},
        remoteVideoDimensions: {}
      };
    },

    /**
     * Registers the actions with the dispatcher that this store is interested
     * in after the initial setup has been performed.
     */
    _registerPostSetupActions: function() {
      this.dispatcher.register(this, [
        "joinRoom"
      ]);
    },

    /**
     * Execute fetchServerData event action from the dispatcher. Although
     * this is to fetch the server data - for rooms on the standalone client,
     * we don't actually need to get any data. Therefore we just save the
     * data that is given to us for when the user chooses to join the room.
     *
     * @param {sharedActions.FetchServerData} actionData
     */
    fetchServerData: function(actionData) {
      if (actionData.windowType !== "room") {
        // Nothing for us to do here, leave it to other stores.
        return;
      }

      this._registerPostSetupActions();

      this.setStoreState({
        roomToken: actionData.token,
        roomState: ROOM_STATES.READY
      });

    },

    /**
     * Handles the action to join to a room.
     */
    joinRoom: function() {
      // Reset the failure reason if necessary.
      if (this.getStoreState().failureReason) {
        this.setStoreState({failureReason: undefined});
      }

      this._setupOutgoingRoom(true);
    },

    /**
     * Sets up an outgoing room. It will try launching the activity to let the
     * FirefoxOS loop app handle the call. If the activity fails:
     *   - if installApp is true, then it'll try to install the FirefoxOS loop
     *     app.
     *   - if installApp is false, then it'll just log and error and fail.
     *
     * @param {boolean} installApp
     */
    _setupOutgoingRoom: function(installApp) {
      var request = new MozActivity({
        name: "room-call",
        data: {
          type: "loop/rToken",
          token: this.getStoreState("roomToken")
        }
      });

      request.onsuccess = function() {};

      request.onerror = (function(event) {
        if (!installApp) {
          // This really should not happen ever.
          console.error(
           "Unexpected activity launch error after the app has been installed");
          return;
        }
        if (event.target.error.name !== "NO_PROVIDER") {
          console.error ("Unexpected " + event.target.error.name);
          return;
        }
        // We need to install the FxOS app.
        this.setStoreState({
            marketplaceSrc: loop.config.marketplaceUrl,
            onMarketplaceMessage: this._onMarketplaceMessage.bind(this)
        });
      }).bind(this);
    },

    /**
     * This method will handle events generated on the marketplace frame. It
     * will launch the FirefoxOS loop app installation, and receive the result
     * of the installation.
     *
     * @param {DOMEvent} event
     */
    _onMarketplaceMessage: function(event) {
      var message = event.data;
      switch (message.name) {
        case "loaded":
          var marketplace = window.document.getElementById("marketplace");
          // Once we have it loaded, we request the installation of the FxOS
          // Loop client app. We will be receiving the result of this action
          // via postMessage from the child iframe.
          marketplace.contentWindow.postMessage({
            "name": "install-package",
            "data": {
              "product": {
                "name": loop.config.fxosApp.name,
                "manifest_url": loop.config.fxosApp.manifestUrl,
                "is_packaged": true
              }
            }
          }, "*");
          break;
        case "install-package":
          window.removeEventListener("message", this.onMarketplaceMessage);
          if (message.error) {
            console.error(message.error.error);
            return;
          }
          // We installed the FxOS app, so we can continue with the call
          // process.
          this._setupOutgoingRoom(false);
          break;
      }
    }
  });

  return FxOSActiveRoomStore;
})();
