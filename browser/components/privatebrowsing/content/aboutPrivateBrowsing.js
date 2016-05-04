/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const FAVICON_QUESTION = "chrome://global/skin/icons/question-32.png";
const FAVICON_PRIVACY = "chrome://browser/skin/privatebrowsing/favicon.svg";

var stringBundle = Services.strings.createBundle(
                    "chrome://browser/locale/aboutPrivateBrowsing.properties");

var prefBranch = Services.prefs.getBranch("privacy.trackingprotection.");
var prefObserver = {
 QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                        Ci.nsISupportsWeakReference]),
 observe: function () {
   let tpSubHeader = document.getElementById("tpSubHeader");
   let tpToggle = document.getElementById("tpToggle");
   let tpButton = document.getElementById("tpButton");
   let title = document.getElementById("title");
   let titleTracking = document.getElementById("titleTracking");
   let globalTrackingEnabled = prefBranch.getBoolPref("enabled");
   let trackingEnabled = globalTrackingEnabled ||
                         prefBranch.getBoolPref("pbmode.enabled");

   tpButton.classList.toggle("hide", globalTrackingEnabled);
   tpToggle.checked = trackingEnabled;
   title.classList.toggle("hide", trackingEnabled);
   titleTracking.classList.toggle("hide", !trackingEnabled);
   tpSubHeader.classList.toggle("tp-off", !trackingEnabled);
 }
};
prefBranch.addObserver("pbmode.enabled", prefObserver, true);
prefBranch.addObserver("enabled", prefObserver, true);

function setFavIcon(url) {
 document.getElementById("favicon").setAttribute("href", url);
}

document.addEventListener("DOMContentLoaded", function () {
 if (!PrivateBrowsingUtils.isContentWindowPrivate(window)) {
   document.documentElement.classList.remove("private");
   document.documentElement.classList.add("normal");
   document.title = stringBundle.GetStringFromName("title.normal");
   document.getElementById("favicon")
           .setAttribute("href", FAVICON_QUESTION);
   document.getElementById("startPrivateBrowsing")
           .addEventListener("command", openPrivateWindow);
   return;
 }

 let tpToggle = document.getElementById("tpToggle");
 document.getElementById("tpButton").addEventListener('click', () => {
   tpToggle.click();
 });

 document.title = stringBundle.GetStringFromName("title.head");
 document.getElementById("favicon")
         .setAttribute("href", FAVICON_PRIVACY);
 tpToggle.addEventListener("change", toggleTrackingProtection);
 document.getElementById("startTour")
         .addEventListener("click", dontShowIntroPanelAgain);

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
                   {bubbles: true}));
}

function dontShowIntroPanelAgain() {
 // Ask chrome to disable the doorhanger
 document.dispatchEvent(
   new CustomEvent("AboutPrivateBrowsingDontShowIntroPanelAgain",
                   {bubbles: true}));
}
