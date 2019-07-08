/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Register the about:debugging URL, that allows to debug various targets such as addons,
// workers and tabs by launching a dedicated DevTools toolbox for the selected target.
// If DevTools are not installed, this about page will display a shim landing page
// encouraging the user to download and install DevTools.
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { nsIAboutModule } = Ci;

function AboutDebugging() {}

AboutDebugging.prototype = {
  classDescription: "about:debugging",
  classID: Components.ID("1060afaf-dc9e-43da-8646-23a2faf48493"),
  contractID: "@mozilla.org/network/protocol/about;1?what=debugging",

  QueryInterface: ChromeUtils.generateQI([nsIAboutModule]),

  newChannel: function(_, loadInfo) {
    const uri = Services.prefs.getBoolPref(
      "devtools.aboutdebugging.new-enabled"
    )
      ? "chrome://devtools/content/aboutdebugging-new/index.html"
      : "chrome://devtools/content/aboutdebugging/aboutdebugging.xhtml";

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

var EXPORTED_SYMBOLS = ["AboutDebugging"];
