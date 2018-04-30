/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
ChromeUtils.defineModuleGetter(this, "AsyncPrefs",
  "resource://gre/modules/AsyncPrefs.jsm");

function nsAboutCapabilities() {
}

nsAboutCapabilities.prototype = {
  init(window) {
    this.window = window;
    try {
      let docShell = window.document.docShell
                           .QueryInterface(Ci.nsIInterfaceRequestor);
      this.mm = docShell.getInterface(Ci.nsIContentFrameMessageManager);
    } catch (e) {
      Cu.reportError(e);
    }
  },

  getBoolPref(aPref, aDefaultValue) {
    return Services.prefs.getBoolPref(aPref, aDefaultValue);
  },

  setBoolPref(aPref, aValue) {
    return new this.window.Promise(function(resolve) {
      AsyncPrefs.set(aPref, aValue).then(function() {
        resolve();
      });
    });
  },

  getCharPref(aPref, aDefaultValue) {
    return Services.prefs.getCharPref(aPref, aDefaultValue);
  },

  setCharPref(aPref, aValue) {
    return new this.window.Promise(function(resolve) {
      AsyncPrefs.set(aPref, aValue).then(function() {
        resolve();
      });
    });
  },

  getStringFromBundle(aStrBundle, aStr) {
    let bundle = Services.strings.createBundle(aStrBundle);
    return bundle.GetStringFromName(aStr);
  },

  formatURLPref(aFormatURL) {
    return Services.urlFormatter.formatURLPref(aFormatURL);
  },

  sendAsyncMessage(aMessage, aParams) {
    this.mm.sendAsyncMessage("AboutCapabilities:" + aMessage, aParams);
  },

  isWindowPrivate() {
    return PrivateBrowsingUtils.isContentWindowPrivate(this.window);
  },

  contractID: "@mozilla.org/aboutcapabilities;1",
  classID: Components.ID("{4c2b1f46-e637-4a91-8108-8a9fb7aab91d}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsAboutCapabilities]);
