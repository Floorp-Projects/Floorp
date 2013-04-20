/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function debug(msg) {
  Services.console.logStringMessage("SessionStoreContent: " + msg);
}

/**
 * Listens for and handles content events that we need for the
 * session store service to be notified of state changes in content.
 */
let EventListener = {

  init: function () {
    addEventListener("pageshow", this, true);
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "pageshow":
        if (event.persisted)
          sendAsyncMessage("SessionStore:pageshow");
        break;
      default:
        debug("received unknown event '" + event.type + "'");
        break;
    }
  }
};

EventListener.init();
