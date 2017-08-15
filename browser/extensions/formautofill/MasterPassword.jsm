/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Helpers for the Master Password Dialog.
 * In the future the Master Password implementation may move here.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "MasterPassword",
];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");


this.MasterPassword = {
  /**
   * Resolve when master password dialogs are closed, immediately if none are open.
   *
   * An existing MP dialog will be focused and will request attention.
   *
   * @returns {Promise<boolean>}
   *          Resolves with whether the user is logged in to MP.
   */
  async waitForExistingDialog() {
    if (!Services.logins.uiBusy) {
      log.debug("waitForExistingDialog: Dialog isn't showing. isLoggedIn:",
                Services.logins.isLoggedIn);
      return Services.logins.isLoggedIn;
    }

    return new Promise((resolve) => {
      log.debug("waitForExistingDialog: Observing the open dialog");
      let observer = {
        QueryInterface: XPCOMUtils.generateQI([
          Ci.nsIObserver,
          Ci.nsISupportsWeakReference,
        ]),

        observe(subject, topic, data) {
          log.debug("waitForExistingDialog: Got notification:", topic);
          // Only run observer once.
          Services.obs.removeObserver(this, "passwordmgr-crypto-login");
          Services.obs.removeObserver(this, "passwordmgr-crypto-loginCanceled");
          if (topic == "passwordmgr-crypto-loginCanceled") {
            resolve(false);
            return;
          }

          resolve(true);
        },
      };

      // Possible leak: it's possible that neither of these notifications
      // will fire, and if that happens, we'll leak the observer (and
      // never return). We should guarantee that at least one of these
      // will fire.
      // See bug XXX.
      Services.obs.addObserver(observer, "passwordmgr-crypto-login");
      Services.obs.addObserver(observer, "passwordmgr-crypto-loginCanceled");

      // Focus and draw attention to the existing master password dialog for the
      // occassions where it's not attached to the current window.
      let promptWin = Services.wm.getMostRecentWindow("prompt:promptPassword");
      promptWin.focus();
      promptWin.getAttention();
    });
  },
};

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = Cu.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
  return new ConsoleAPI({
    maxLogLevelPref: "masterPassword.loglevel",
    prefix: "Master Password",
  });
});
