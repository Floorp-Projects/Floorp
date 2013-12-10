/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Helper class for IndexedDB, parent part. Listens to
 * messages from the child and shows prompts for them.
 */
let IndexedDB = {
  _permissionsPrompt: "indexedDB-permissions-prompt",
  _permissionsResponse: "indexedDB-permissions-response",

  _quotaPrompt: "indexedDB-quota-prompt",
  _quotaResponse: "indexedDB-quota-response",
  _quotaCancel: "indexedDB-quota-cancel",

  _notificationIcon: "indexedDB-notification-icon",

  receiveMessage: function(aMessage) {
    switch (aMessage.name) {
      case "IndexedDB:Prompt":
        this.showPrompt(aMessage);
    }
  },

  showPrompt: function(aMessage) {
    let browser = aMessage.target;
    let payload = aMessage.json;
    let host = payload.host;
    let topic = payload.topic;
    let type;

    if (topic == this._permissionsPrompt) {
      type = "indexedDB";
      payload.responseTopic = this._permissionsResponse;
    } else if (topic == this._quotaPrompt) {
      type = "indexedDBQuota";
      payload.responseTopic = this._quotaResponse;
    } else if (topic == this._quotaCancel) {
      payload.permission = Ci.nsIPermissionManager.UNKNOWN_ACTION;
      browser.messageManager.sendAsyncMessage("IndexedDB:Response", payload);
      // XXX Need to actually save this?
      return;
    }

    let prompt = Cc["@mozilla.org/content-permission/prompt;1"].createInstance(Ci.nsIContentPermissionPrompt);

    // If the user waits a long time before responding, we default to UNKNOWN_ACTION.
    let timeoutId = setTimeout(function() {
      payload.permission = Ci.nsIPermissionManager.UNKNOWN_ACTION;
      browser.messageManager.sendAsyncMessage("IndexedDB:Response", payload);
      timeoutId = null;
    }, 30000);
 
    function checkTimeout() {
      if (timeoutId === null) return true;
      clearTimeout(timeoutId);
      timeoutId = null;
      return false;
    }

    prompt.prompt({
      type: type,
      uri: Services.io.newURI(payload.location, null, null),
      window: null,
      element: aMessage.target,

      cancel: function() {
        if (checkTimeout()) return;
        payload.permission = Ci.nsIPermissionManager.DENY_ACTION;
        browser.messageManager.sendAsyncMessage("IndexedDB:Response", payload);
      },

      allow: function() {
        if (checkTimeout()) return;
        payload.permission = Ci.nsIPermissionManager.ALLOW_ACTION;
        browser.messageManager.sendAsyncMessage("IndexedDB:Response", payload);
      },
    });
  },
};

