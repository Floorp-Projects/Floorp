/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cm = Components.manager;

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

// See LOG_LEVELS in Console.jsm. Common examples: "All", "Info", "Warn", & "Error".
const PREF_LOG_LEVEL = "loop.debug.loglevel";

// eslint-disable-next-line no-unused-vars
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = ChromeUtils.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
  let consoleOptions = {
    maxLogLevelPref: PREF_LOG_LEVEL,
    prefix: "Loop"
  };
  return new ConsoleAPI(consoleOptions);
});


function AboutPage(chromeURL, aboutHost, classID, description, uriFlags) {
  this.chromeURL = chromeURL;
  this.aboutHost = aboutHost;
  this.classID = Components.ID(classID);
  this.description = description;
  this.uriFlags = uriFlags;
}

AboutPage.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAboutModule]),
  getURIFlags(aURI) { // eslint-disable-line no-unused-vars
    return this.uriFlags;
  },

  newChannel(aURI, aLoadInfo) {
    let newURI = Services.io.newURI(this.chromeURL);
    let channel = Services.io.newChannelFromURIWithLoadInfo(newURI,
                                                            aLoadInfo);
    channel.originalURI = aURI;

    if (this.uriFlags & Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT) {
      let principal = Services.scriptSecurityManager.createCodebasePrincipal(aURI, {});
      channel.owner = principal;
    }
    return channel;
  },

  createInstance(outer, iid) {
    if (outer !== null) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(iid);
  },

  register() {
    Cm.QueryInterface(Ci.nsIComponentRegistrar).registerFactory(
      this.classID, this.description,
      "@mozilla.org/network/protocol/about;1?what=" + this.aboutHost, this);
  },

  unregister() {
    Cm.QueryInterface(Ci.nsIComponentRegistrar).unregisterFactory(
      this.classID, this);
  }
};

/* exported AboutPocket */
var AboutPocket = {};

XPCOMUtils.defineLazyGetter(AboutPocket, "aboutSaved", () =>
  new AboutPage("chrome://pocket/content/panels/saved.html",
                "pocket-saved",
                "{3e759f54-37af-7843-9824-f71b5993ceed}",
                "About Pocket Saved",
                Ci.nsIAboutModule.ALLOW_SCRIPT |
                Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT |
                Ci.nsIAboutModule.HIDE_FROM_ABOUTABOUT)
);

XPCOMUtils.defineLazyGetter(AboutPocket, "aboutSignup", () =>
  new AboutPage("chrome://pocket/content/panels/signup.html",
                "pocket-signup",
                "{8548329d-00c4-234e-8f17-75026db3b56e}",
                "About Pocket Signup",
                Ci.nsIAboutModule.ALLOW_SCRIPT |
                Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT |
                Ci.nsIAboutModule.HIDE_FROM_ABOUTABOUT)
);

var EXPORTED_SYMBOLS = ["AboutPocket"];
