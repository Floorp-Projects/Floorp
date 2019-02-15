/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This component is only registered and packaged for local and nightly builds in order to
// open the new about:debugging when going to about:debugging-new, without having to flip
// the preference. This allows running both versions of about:debugging side by side to
// compare them.
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { nsIAboutModule } = Ci;

function AboutDebuggingNew() {}

AboutDebuggingNew.prototype = {
  classDescription: "about:debugging-new",
  classID: Components.ID("610e4e26-26bd-4a7d-aebc-69211d5a3be0"),
  contractID: "@mozilla.org/network/protocol/about;1?what=debugging-new",

  QueryInterface: ChromeUtils.generateQI([nsIAboutModule]),

  newChannel: function(_, loadInfo) {
    const uri = "chrome://devtools/content/aboutdebugging-new/index.html";

    const chan = Services.io.newChannelFromURIWithLoadInfo(
      Services.io.newURI(uri),
      loadInfo
    );
    chan.owner = Services.scriptSecurityManager.getSystemPrincipal();
    return chan;
  },

  getURIFlags: function(uri) {
    return nsIAboutModule.ALLOW_SCRIPT;
  },
};

var EXPORTED_SYMBOLS = ["AboutDebuggingNew"];
