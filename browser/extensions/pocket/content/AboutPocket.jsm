/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { interfaces: Ci, results: Cr, manager: Cm, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// See LOG_LEVELS in Console.jsm. Common examples: "All", "Info", "Warn", & "Error".
const PREF_LOG_LEVEL = "loop.debug.loglevel";

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = Cu.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
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
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutModule]),
  getURIFlags: function(aURI) { // eslint-disable-line no-unused-vars
    return this.uriFlags;
  },

  newChannel: function(aURI, aLoadInfo) {
    let newURI = Services.io.newURI(this.chromeURL, null, null);
    let channel = Services.io.newChannelFromURIWithLoadInfo(newURI,
                                                            aLoadInfo);
    channel.originalURI = aURI;

    if (this.uriFlags & Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT) {
      let principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(aURI);
      channel.owner = principal;
    }
    return channel;
  },

  createInstance: function(outer, iid) {
    if (outer !== null) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(iid);
  },

  register: function() {
    Cm.QueryInterface(Ci.nsIComponentRegistrar).registerFactory(
      this.classID, this.description,
      "@mozilla.org/network/protocol/about;1?what=" + this.aboutHost, this);
  },

  unregister: function() {
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

this.EXPORTED_SYMBOLS = ["AboutPocket"];
