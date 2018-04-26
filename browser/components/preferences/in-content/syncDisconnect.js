// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  SyncDisconnect: "resource://services-sync/SyncDisconnect.jsm",
});

let gSyncDisconnectDialog = {
  init() {
    if (SyncDisconnect.promiseDisconnectFinished) {
      // There's a sanitization under way - just re-show our "waiting" state
      // and close the dialog when it's complete.
      this.waitForCompletion(SyncDisconnect.promiseDisconnectFinished);
    }
  },

  // when either of the checkboxes are changed.
  onDeleteOptionChange() {
    let eitherChecked = document.getElementById("deleteRemoteSyncData").checked ||
                        document.getElementById("deleteRemoteOtherData").checked;
    let newTitle = eitherChecked ? "sync-disconnect-confirm-disconnect-delete" :
                                   "sync-disconnect-confirm-disconnect";
    let butDisconnect = document.getElementById("butDisconnect");
    document.l10n.setAttributes(butDisconnect, newTitle);
  },

  accept(event) {
    let options = {
      sanitizeSyncData: document.getElementById("deleteRemoteSyncData").checked,
      sanitizeBrowserData: document.getElementById("deleteRemoteOtherData").checked,
    };

    // And do the santize.
    this.waitForCompletion(SyncDisconnect.disconnect(options));
  },

  waitForCompletion(promiseComplete) {
    // Change the dialog to show we are waiting for completion.
    document.getElementById("deleteOptionsContent").hidden = true;
    document.getElementById("deletingContent").hidden = false;

    // And do the santize.
    promiseComplete.catch(ex => {
      console.error("Failed to sanitize", ex);
    }).then(() => {
      // We dispatch a custom event so the caller knows how we were closed
      // (if we were a dialog we'd get this for free, but we'd also lose the
      // ability to keep the dialog open while the sanitize was running)
      let closingEvent = new CustomEvent("dialogclosing", {
        bubbles: true,
        detail: {button: "accept"},
      });
      document.documentElement.dispatchEvent(closingEvent);
      close();
    });
  }
};
