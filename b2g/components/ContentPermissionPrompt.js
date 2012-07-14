/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {

  handleExistingPermission: function handleExistingPermission(request) {
    let result = Services.perms.testExactPermission(request.uri, request.type);
    if (result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      request.allow();
      return true;
    }
    if (result == Ci.nsIPermissionManager.DENY_ACTION) {
      request.cancel();
      return true;
    }
    return false;
  },

  _id: 0,
  prompt: function(request) {
    // returns true if the request was handled
    if (this.handleExistingPermission(request))
       return;

    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    let content = browser.getContentWindow();
    if (!content)
      return;

    let requestId = this._id++;
    content.addEventListener("mozContentEvent", function contentEvent(evt) {
      if (evt.detail.id != requestId)
        return;
      evt.target.removeEventListener(evt.type, contentEvent);

      if (evt.detail.type == "permission-allow") {
        request.allow();
        return;
      }

      request.cancel();
    });

    let details = {
      "type": "permission-prompt",
      "permission": request.type,
      "id": requestId,
      "url": request.uri.spec
    };
    let event = content.document.createEvent("CustomEvent");
    event.initCustomEvent("mozChromeEvent", true, true, details);
    content.dispatchEvent(event);
  },

  classID: Components.ID("{8c719f03-afe0-4aac-91ff-6c215895d467}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionPrompt])
};


//module initialization
const NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentPermissionPrompt]);
