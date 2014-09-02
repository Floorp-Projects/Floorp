/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

// Small helper to expose nsICommandLine object to chrome code

function CommandlineHandler() {
  this.wrappedJSObject = this;
}

CommandlineHandler.prototype = {
    handle: function(cmdLine) {
      this.cmdLine = cmdLine;
    },

    helpInfo: "",
    classID: Components.ID("{385993fe-8710-4621-9fb1-00a09d8bec37}"),
    QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([CommandlineHandler]);
