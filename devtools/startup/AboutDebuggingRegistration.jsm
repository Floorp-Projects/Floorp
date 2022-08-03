/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Register the about:debugging URL, that allows to debug tabs, extensions, workers on
// the current instance of Firefox or on a remote Firefox.

const { nsIAboutModule } = Ci;

function AboutDebugging() {}

AboutDebugging.prototype = {
  classDescription: "about:debugging",
  classID: Components.ID("1060afaf-dc9e-43da-8646-23a2faf48493"),
  contractID: "@mozilla.org/network/protocol/about;1?what=debugging",

  QueryInterface: ChromeUtils.generateQI([nsIAboutModule]),

  newChannel(_, loadInfo) {
    const chan = Services.io.newChannelFromURIWithLoadInfo(
      Services.io.newURI("chrome://devtools/content/aboutdebugging/index.html"),
      loadInfo
    );
    chan.owner = Services.scriptSecurityManager.getSystemPrincipal();
    return chan;
  },

  getURIFlags(uri) {
    return nsIAboutModule.ALLOW_SCRIPT | nsIAboutModule.IS_SECURE_CHROME_UI;
  },

  getChromeURI(_uri) {
    return Services.io.newURI(
      "chrome://devtools/content/aboutdebugging/index.html"
    );
  },
};

var EXPORTED_SYMBOLS = ["AboutDebugging"];
