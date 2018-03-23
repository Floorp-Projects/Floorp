/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const FAVICON_QUESTION = "chrome://global/skin/icons/question-32.png";
const STRING_BUNDLE = "chrome://browser/locale/aboutPrivateBrowsing.properties";
const TP_ENABLED_PREF = "privacy.trackingprotection.enabled";
const TP_PB_ENABLED_PREF = "privacy.trackingprotection.pbmode.enabled";

function updateTPInfo() {
  let aboutCapabilities = document.aboutCapabilities;
  let tpButton = document.getElementById("tpButton");
  let tpToggle = document.getElementById("tpToggle");
  let title = document.getElementById("title");
  let titleTracking = document.getElementById("titleTracking");
  let tpSubHeader = document.getElementById("tpSubHeader");

  let globalTrackingEnabled = aboutCapabilities.getBoolPref(TP_ENABLED_PREF, null);
  let trackingEnabled = globalTrackingEnabled ||
                        aboutCapabilities.getBoolPref(TP_PB_ENABLED_PREF, null);

  // if tracking protection is enabled globally we don't even give the user
  // a choice here by hiding the toggle completely.
  tpButton.classList.toggle("hide", globalTrackingEnabled);
  tpToggle.checked = trackingEnabled;
  title.classList.toggle("hide", trackingEnabled);
  titleTracking.classList.toggle("hide", !trackingEnabled);
  tpSubHeader.classList.toggle("tp-off", !trackingEnabled);
}

document.addEventListener("DOMContentLoaded", function() {
  let aboutCapabilities = document.aboutCapabilities;
  if (!aboutCapabilities.isWindowPrivate()) {
    document.documentElement.classList.remove("private");
    document.documentElement.classList.add("normal");
    document.getElementById("favicon").setAttribute("href", FAVICON_QUESTION);
    document.getElementById("startPrivateBrowsing").addEventListener("click", function() {
      aboutCapabilities.sendAsyncMessage("OpenPrivateWindow", null);
    });
    return;
  }

  document.getElementById("startTour").addEventListener("click", function() {
    aboutCapabilities.sendAsyncMessage("DontShowIntroPanelAgain", null);
  });
  document.getElementById("startTour").setAttribute("href",
    aboutCapabilities.formatURLPref("privacy.trackingprotection.introURL"));
  document.getElementById("learnMore").setAttribute("href",
    aboutCapabilities.formatURLPref("app.support.baseURL") + "private-browsing");

  let tpToggle = document.getElementById("tpToggle");
  document.getElementById("tpButton").addEventListener("click", () => {
    tpToggle.click();
  });
  tpToggle.addEventListener("change", function() {
    aboutCapabilities.setBoolPref(TP_PB_ENABLED_PREF, tpToggle.checked).then(function() {
      updateTPInfo();
    });
  });

  updateTPInfo();
});
