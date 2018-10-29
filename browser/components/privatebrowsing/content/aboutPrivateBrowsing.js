/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

const TP_PB_ENABLED_PREF = "privacy.trackingprotection.pbmode.enabled";

document.addEventListener("DOMContentLoaded", function() {
  if (!RPMIsWindowPrivate()) {
    document.documentElement.classList.remove("private");
    document.documentElement.classList.add("normal");
    document.getElementById("startPrivateBrowsing").addEventListener("click", function() {
      RPMSendAsyncMessage("OpenPrivateWindow");
    });
    return;
  }

  document.getElementById("startTour").addEventListener("click", function() {
    RPMSendAsyncMessage("DontShowIntroPanelAgain");
  });

  let introURL = RPMGetFormatURLPref("privacy.trackingprotection.introURL");
  // Variation 1 is specific to the Content Blocking UI.
  let variation = "?variation=1";

  document.getElementById("startTour").setAttribute("href", introURL + variation);

  document.getElementById("learnMore").setAttribute("href",
    RPMGetFormatURLPref("app.support.baseURL") + "private-browsing");

  let tpEnabled = RPMGetBoolPref(TP_PB_ENABLED_PREF);
  if (!tpEnabled) {
    document.getElementById("tpSubHeader").remove();
    document.getElementById("tpSection").remove();
  }
});
