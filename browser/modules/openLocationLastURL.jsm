/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const LAST_URL_PREF = "general.open_location.last_url";
const nsISupportsString = Components.interfaces.nsISupportsString;

var EXPORTED_SYMBOLS = [ "gOpenLocationLastURL" ];

let pbSvc = Components.classes["@mozilla.org/privatebrowsing;1"]
                      .getService(Components.interfaces.nsIPrivateBrowsingService);
let prefSvc = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefBranch);

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
      case "private-browsing":
        gOpenLocationLastURLData = "";
        break;
      case "browser:purge-session-history":
        gOpenLocationLastURL.reset();
        break;
    }
  }
};

let os = Components.classes["@mozilla.org/observer-service;1"]
                   .getService(Components.interfaces.nsIObserverService);
os.addObserver(observer, "private-browsing", true);
os.addObserver(observer, "browser:purge-session-history", true);

let gOpenLocationLastURLData = "";
let gOpenLocationLastURL = {
  get value() {
    if (pbSvc.privateBrowsingEnabled)
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
    if (pbSvc.privateBrowsingEnabled)
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
