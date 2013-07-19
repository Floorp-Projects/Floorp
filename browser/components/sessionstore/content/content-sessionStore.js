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

  DOM_EVENTS: [
    "pageshow", "change", "input"
  ],

  init: function () {
    this.DOM_EVENTS.forEach(e => addEventListener(e, this, true));
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "pageshow":
        if (event.persisted)
          sendAsyncMessage("SessionStore:pageshow");
        break;
      case "input":
      case "change":
        sendAsyncMessage("SessionStore:input");
        break;
      default:
        debug("received unknown event '" + event.type + "'");
        break;
    }
  }
};

EventListener.init();
