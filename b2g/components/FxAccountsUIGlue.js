/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/ContentRequestHelper.jsm");

function FxAccountsUIGlue() {
}

FxAccountsUIGlue.prototype = {

  __proto__: ContentRequestHelper.prototype,

  signInFlow: function() {
    return this.contentRequest("mozFxAccountsRPContentEvent",
                               "mozFxAccountsUnsolChromeEvent",
                               "openFlow");
  },

  refreshAuthentication: function(aEmail) {
    return this.contentRequest("mozFxAccountsRPContentEvent",
                               "mozFxAccountsUnsolChromeEvent",
                               "refreshAuthentication", {
      email: aEmail
    });
  },

  classID: Components.ID("{51875c14-91d7-4b8c-b65d-3549e101228c}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFxAccountsUIGlue])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FxAccountsUIGlue]);
