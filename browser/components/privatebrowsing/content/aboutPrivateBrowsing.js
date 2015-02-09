/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

var stringBundle = Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci.nsIStringBundleService)
                                                         .createBundle("chrome://browser/locale/aboutPrivateBrowsing.properties");

if (!PrivateBrowsingUtils.isContentWindowPrivate(window)) {
  document.title = stringBundle.GetStringFromName("title.normal");
  setFavIcon("chrome://global/skin/icons/question-16.png");
} else {
  document.title = stringBundle.GetStringFromName("title");
  setFavIcon("chrome://browser/skin/Privacy-16.png");
}

function setFavIcon(url) {
  var icon = document.createElement("link");
  icon.setAttribute("rel", "icon");
  icon.setAttribute("type", "image/png");
  icon.setAttribute("href", url);
  var head = document.getElementsByTagName("head")[0];
  head.insertBefore(icon, head.firstChild);
}

document.addEventListener("DOMContentLoaded", function () {
  if (!PrivateBrowsingUtils.isContentWindowPrivate(window)) {
    document.body.setAttribute("class", "normal");
  }

  // Set up the help link
  let learnMoreURL = Cc["@mozilla.org/toolkit/URLFormatterService;1"]
                     .getService(Ci.nsIURLFormatter)
                     .formatURLPref("app.support.baseURL");
  let learnMore = document.getElementById("learnMore");
  if (learnMore) {
    learnMore.setAttribute("href", learnMoreURL + "private-browsing");
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
