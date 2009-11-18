/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Open Location Dialog Utility Code.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari <ehsan.akhgari@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    if (prefSvc.prefHasUserValue(LAST_URL_PREF))
        prefSvc.clearUserPref(LAST_URL_PREF);
    gOpenLocationLastURLData = "";
  }
};
