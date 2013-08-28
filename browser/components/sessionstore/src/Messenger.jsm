/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Messenger"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);

/**
 * The external API exported by this module.
 */
this.Messenger = Object.freeze({
  send: function (tab, type, options = {}) {
    return MessengerInternal.send(tab, type, options);
  }
});

/**
 * A module that handles communication between the main and content processes.
 */
let MessengerInternal = {
  // The id of the last message we sent. This is used to assign a unique ID to
  // every message we send to handle multiple responses from the same browser.
  _latestMessageID: 0,

  /**
   * Sends a message to the given tab and waits for a response.
   *
   * @param tab
   *        tabbrowser tab
   * @param type
   *        {string} the type of the message
   * @param options (optional)
   *        {timeout: int} to set the timeout in milliseconds
   * @return {Promise} A promise that will resolve to the response message or
   *                   be reject when timing out.
   */
  send: function (tab, type, options = {}) {
    let browser = tab.linkedBrowser;
    let mm = browser.messageManager;
    let deferred = Promise.defer();
    let id = ++this._latestMessageID;
    let timeout;

    function onMessage({data: {id: mid, data}}) {
      if (mid == id) {
        mm.removeMessageListener(type, onMessage);
        clearTimeout(timeout);
        deferred.resolve(data);
      }
    }

    mm.addMessageListener(type, onMessage);
    mm.sendAsyncMessage(type, {id: id});

    function onTimeout() {
      mm.removeMessageListener(type, onMessage);
      deferred.reject(new Error("Timed out while waiting for a " + type + " " +
                                "response message."));
    }

    let delay = (options && options.timeout) || 5000;
    timeout = setTimeout(onTimeout, delay);
    return deferred.promise;
  }
};
