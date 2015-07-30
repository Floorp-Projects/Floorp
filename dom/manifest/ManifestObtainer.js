/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ManifestObtainer is an implementation of:
 * http://w3c.github.io/manifest/#obtaining
 *
 * Exposes public method `.obtainManifest(browserWindow)`, which returns
 * a promise. If successful, you get back a manifest (string).
 *
 * For e10s compat, this JSM relies on the following to do
 * the nessesary IPC:
 *   dom/ipc/manifestMessages.js
 *
 * whose internal URL is:
 *   'chrome://global/content/manifestMessages.js'
 *
 * Which is injected into every browser instance via browser.js.
 *
 * BUG: https://bugzilla.mozilla.org/show_bug.cgi?id=1083410
 * exported ManifestObtainer
 */
'use strict';
const MSG_KEY = 'DOM:ManifestObtainer:Obtain';
let messageCounter = 0;
// FIXME: Ideally, we would store a reference to the
//        message manager in a weakmap instead of needing a
//        browserMap. However, trying to store a messageManager
//        results in a TypeError because of:
//        https://bugzilla.mozilla.org/show_bug.cgi?id=888600
const browsersMap = new WeakMap();

function ManifestObtainer() {}

ManifestObtainer.prototype = {
  obtainManifest(aBrowserWindow) {
    if (!aBrowserWindow) {
      const err = new TypeError('Invalid input. Expected xul browser.');
      return Promise.reject(err);
    }
    const mm = aBrowserWindow.messageManager;
    const onMessage = function(aMsg) {
      const msgId = aMsg.data.msgId;
      const {
        resolve, reject
      } = browsersMap.get(aBrowserWindow).get(msgId);
      browsersMap.get(aBrowserWindow).delete(msgId);
      // If we we've processed all messages,
      // stop listening.
      if (!browsersMap.get(aBrowserWindow).size) {
        browsersMap.delete(aBrowserWindow);
        mm.removeMessageListener(MSG_KEY, onMessage);
      }
      if (aMsg.data.success) {
        return resolve(aMsg.data.result);
      }
      reject(toError(aMsg.data.result));
    };
    // If we are not already listening for messages
    // start listening.
    if (!browsersMap.has(aBrowserWindow)) {
      browsersMap.set(aBrowserWindow, new Map());
      mm.addMessageListener(MSG_KEY, onMessage);
    }
    return new Promise((resolve, reject) => {
      const msgId = messageCounter++;
      browsersMap.get(aBrowserWindow).set(msgId, {
        resolve: resolve,
        reject: reject
      });
      mm.sendAsyncMessage(MSG_KEY, {
        msgId: msgId
      });
    });

    function toError(aErrorClone) {
      let error;
      switch (aErrorClone.name) {
      case 'TypeError':
        error = new TypeError();
        break;
      default:
        error = new Error();
      }
      Object.getOwnPropertyNames(aErrorClone)
        .forEach(name => error[name] = aErrorClone[name]);
      return error;
    }
  }
};
this.ManifestObtainer = ManifestObtainer; // jshint ignore:line
this.EXPORTED_SYMBOLS = ['ManifestObtainer']; // jshint ignore:line
