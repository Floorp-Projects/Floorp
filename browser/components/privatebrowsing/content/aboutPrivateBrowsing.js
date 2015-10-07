/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const FAVICON_QUESTION = "chrome://global/skin/icons/question-16.png";
const FAVICON_PRIVACY = "chrome://browser/skin/Privacy-16.png";

var stringBundle = Services.strings.createBundle(
                     "chrome://browser/locale/aboutPrivateBrowsing.properties");

var prefBranch = Services.prefs.getBranch("privacy.trackingprotection.");
var prefObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),
  observe: function () {
    if (prefBranch.getBoolPref("enabled")) {
      document.body.setAttribute("globalTpEnabled", "true");
    } else {
      document.body.removeAttribute("globalTpEnabled");
    }
    if (prefBranch.getBoolPref("pbmode.enabled") ||
        prefBranch.getBoolPref("enabled")) {
      document.body.setAttribute("tpEnabled", "true");
    } else {
      document.body.removeAttribute("tpEnabled");
    }
  },
};
prefBranch.addObserver("pbmode.enabled", prefObserver, true);
prefBranch.addObserver("enabled", prefObserver, true);

function setFavIcon(url) {
  document.getElementById("favicon").setAttribute("href", url);
}

document.addEventListener("DOMContentLoaded", function () {
  if (!PrivateBrowsingUtils.isContentWindowPrivate(window)) {
    document.body.setAttribute("class", "normal");
    document.title = stringBundle.GetStringFromName("title.normal");
    document.getElementById("favicon")
            .setAttribute("href", FAVICON_QUESTION);
    document.getElementById("startPrivateBrowsing")
            .addEventListener("command", openPrivateWindow);
    return;
  }

  document.title = stringBundle.GetStringFromName("title");
  document.getElementById("favicon")
          .setAttribute("href", FAVICON_PRIVACY);
  document.getElementById("enableTrackingProtection")
          .addEventListener("click", toggleTrackingProtection);
  document.getElementById("disableTrackingProtection")
          .addEventListener("click", toggleTrackingProtection);

  let formatURLPref = Cc["@mozilla.org/toolkit/URLFormatterService;1"]
                        .getService(Ci.nsIURLFormatter).formatURLPref;
  document.getElementById("startTour").setAttribute("href",
                     formatURLPref("privacy.trackingprotection.introURL"));
  document.getElementById("learnMore").setAttribute("href",
                     formatURLPref("app.support.baseURL") + "private-browsing");

  // Update state that depends on preferences.
  prefObserver.observe();
}, false);

function openPrivateWindow() {
  // Ask chrome to open a private window
  document.dispatchEvent(
    new CustomEvent("AboutPrivateBrowsingOpenWindow", {bubbles:true}));
}

function toggleTrackingProtection() {
  // Ask chrome to enable tracking protection
  document.dispatchEvent(
    new CustomEvent("AboutPrivateBrowsingToggleTrackingProtection",
                    {bubbles:true}));
}
