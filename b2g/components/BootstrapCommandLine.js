/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function BootstrapCommandlineHandler() {
  this.wrappedJSObject = this;
  this.startManifestURL = null;
}

BootstrapCommandlineHandler.prototype = {
    bailout: function(aMsg) {
      dump("************************************************************\n");
      dump("* /!\\ " + aMsg + "\n");
      dump("************************************************************\n");
      let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"]
                         .getService(Ci.nsIAppStartup);
      appStartup.quit(appStartup.eForceQuit);
    },

    handle: function(aCmdLine) {
      this.startManifestURL = null;

      try {
        // Returns null if the argument was not specified.  Throws
        // NS_ERROR_INVALID_ARG if there is no parameter specified (because
        // it was the last argument or the next argument starts with '-').
        // However, someone could still explicitly pass an empty argument!
        this.startManifestURL = aCmdLine.handleFlagWithParam("start-manifest", false);
      } catch(e) {
        return;
      }

      if (!this.startManifestURL) {
        return;
      }
    },

    helpInfo: "--start-manifest=manifest_url",
    classID: Components.ID("{fd663ec8-cf3f-4c2b-aacb-17a6915ccb44}"),
    QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([BootstrapCommandlineHandler]);
