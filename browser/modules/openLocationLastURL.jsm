/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const LAST_URL_PREF = "general.open_location.last_url";
const nsISupportsString = Components.interfaces.nsISupportsString;
const Ci = Components.interfaces;

var EXPORTED_SYMBOLS = [ "OpenLocationLastURL" ];

let prefSvc = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefBranch);
let gOpenLocationLastURLData = "";

let observer = {
  QueryInterface: function (aIID) {
    if (aIID.equals(Components.interfaces.nsIObserver) ||
        aIID.equals(Components.interfaces.nsISupports) ||
        aIID.equals(Components.interfaces.nsISupportsWeakReference))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },
  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "last-pb-context-exited":
        gOpenLocationLastURLData = "";
        break;
      case "browser:purge-session-history":
        prefSvc.clearUserPref(LAST_URL_PREF);
        gOpenLocationLastURLData = "";
        break;
    }
  }
};

let os = Components.classes["@mozilla.org/observer-service;1"]
                   .getService(Components.interfaces.nsIObserverService);
os.addObserver(observer, "last-pb-context-exited", true);
os.addObserver(observer, "browser:purge-session-history", true);


function OpenLocationLastURL(aWindow) {
  this.window = aWindow;
}

OpenLocationLastURL.prototype = {
  isPrivate: function OpenLocationLastURL_isPrivate() {
    // Assume not in private browsing mode, unless the browser window is
    // in private mode.
    if (!this.window || !("gPrivateBrowsingUI" in this.window))
      return false;
  
    return this.window.gPrivateBrowsingUI.privateWindow;
  },
  get value() {
    if (this.isPrivate())
      return gOpenLocationLastURLData;
    else {
      try {
        return prefSvc.getComplexValue(LAST_URL_PREF, nsISupportsString).data;
      }
      catch (e) {
        return "";
      }
    }
  },
  set value(val) {
    if (typeof val != "string")
      val = "";
    if (this.isPrivate())
      gOpenLocationLastURLData = val;
    else {
      let str = Components.classes["@mozilla.org/supports-string;1"]
                          .createInstance(Components.interfaces.nsISupportsString);
      str.data = val;
      prefSvc.setComplexValue(LAST_URL_PREF, nsISupportsString, str);
    }
  },
  reset: function() {
    prefSvc.clearUserPref(LAST_URL_PREF);
    gOpenLocationLastURLData = "";
  }
};
