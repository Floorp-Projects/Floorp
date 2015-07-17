/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

let stringBundle = Services.strings.createBundle(
                     "chrome://browser/locale/aboutPrivateBrowsing.properties");

let useTour = false;
try {
  useTour = Services.prefs.getBoolPref("privacy.trackingprotection.ui.enabled");
} catch (ex) {
  // The preference is not available.
}

if (!PrivateBrowsingUtils.isContentWindowPrivate(window)) {
  document.title = stringBundle.GetStringFromName("title.normal");
  setFavIcon("chrome://global/skin/icons/question-16.png");
} else {
  document.title = stringBundle.GetStringFromName("title");
  setFavIcon("chrome://browser/skin/Privacy-16.png");
}

function setFavIcon(url) {
  let icon = document.createElement("link");
  icon.setAttribute("rel", "icon");
  icon.setAttribute("type", "image/png");
  icon.setAttribute("href", url);
  let head = document.getElementsByTagName("head")[0];
  head.insertBefore(icon, head.firstChild);
}

document.addEventListener("DOMContentLoaded", function () {
  let formatURLPref = Cc["@mozilla.org/toolkit/URLFormatterService;1"]
                        .getService(Ci.nsIURLFormatter).formatURLPref;
  let learnMoreURL = formatURLPref("app.support.baseURL") + "private-browsing";

  if (!PrivateBrowsingUtils.isContentWindowPrivate(window)) {
    // Normal browsing window.
    document.body.setAttribute("class", "normal");
  } else if (!useTour) {
    // Private browsing window, classic version.
    document.getElementById("learnMore").setAttribute("href", learnMoreURL);
  } else {
    // Private browsing window, Tracking Protection tour version.
    document.body.setAttribute("class", "tour");
    let tourURL = formatURLPref("privacy.trackingprotection.introURL");
    document.getElementById("startTour").setAttribute("href", tourURL);
    document.getElementById("tourLearnMore").setAttribute("href", learnMoreURL);
  }

  let startPrivateBrowsing = document.getElementById("startPrivateBrowsing");
  if (startPrivateBrowsing) {
    startPrivateBrowsing.addEventListener("command", openPrivateWindow);
  }
}, false);

function openPrivateWindow() {
  // Ask chrome to open a private window
  document.dispatchEvent(
    new CustomEvent("AboutPrivateBrowsingOpenWindow", {bubbles:true}));
}
