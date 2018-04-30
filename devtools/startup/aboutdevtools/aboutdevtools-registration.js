/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Register the about:devtools URL, that is opened whenever a user attempts to open
// DevTools for the first time.
const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", {});
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});

const { nsIAboutModule } = Ci;

function AboutDevtools() {}

AboutDevtools.prototype = {
  uri: Services.io.newURI("chrome://devtools-startup/content/aboutdevtools/aboutdevtools.xhtml"),
  classDescription: "about:devtools",
  classID: Components.ID("3a16d383-92bd-4c24-ac10-0e2bd66883ab"),
  contractID: "@mozilla.org/network/protocol/about;1?what=devtools",

  QueryInterface: ChromeUtils.generateQI([nsIAboutModule]),

  newChannel: function(uri, loadInfo) {
    let chan = Services.io.newChannelFromURIWithLoadInfo(
      this.uri,
      loadInfo
    );
    chan.owner = Services.scriptSecurityManager.getSystemPrincipal();
    return chan;
  },

  getURIFlags: function(uri) {
    return nsIAboutModule.ALLOW_SCRIPT;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([
  AboutDevtools
]);
