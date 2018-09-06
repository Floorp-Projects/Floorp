/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

const FAVICON_QUESTION = "chrome://global/skin/icons/question-32.png";
const CB_ENABLED_PREF = "browser.contentblocking.enabled";
const CB_UI_ENABLED_PREF = "browser.contentblocking.ui.enabled";
const TP_ENABLED_PREF = "privacy.trackingprotection.enabled";
const TP_PB_ENABLED_PREF = "privacy.trackingprotection.pbmode.enabled";

let contentBlockingUIEnabled = false;

function updateTPInfo() {
  let tpButton = document.getElementById("tpButton");
  let tpToggle = document.getElementById("tpToggle");
  let title = document.getElementById("title");
  let titleTracking = document.getElementById("titleTracking");
  let tpSubHeader = document.getElementById("tpSubHeader");

  let tpTitle = document.getElementById("tpTitle");
  let cbTitle = document.getElementById("cbTitle");
  let tpDescription = document.getElementById("tpDescription");
  let cbDescription = document.getElementById("cbDescription");

  tpTitle.classList.toggle("hide", contentBlockingUIEnabled);
  tpDescription.classList.toggle("hide", contentBlockingUIEnabled);

  cbTitle.classList.toggle("hide", !contentBlockingUIEnabled);
  cbDescription.classList.toggle("hide", !contentBlockingUIEnabled);

  let globalTrackingEnabled = RPMGetBoolPref(TP_ENABLED_PREF);
  let trackingEnabled = globalTrackingEnabled || RPMGetBoolPref(TP_PB_ENABLED_PREF);

  if (contentBlockingUIEnabled) {
    let contentBlockingEnabled = RPMGetBoolPref(CB_ENABLED_PREF);
    trackingEnabled = trackingEnabled && contentBlockingEnabled;
  } else {
    title.classList.toggle("hide", trackingEnabled);
    titleTracking.classList.toggle("hide", !trackingEnabled);
  }

  // if tracking protection is enabled globally we don't even give the user
  // a choice here by hiding the toggle completely.
  tpButton.classList.toggle("hide", globalTrackingEnabled);
  tpToggle.checked = trackingEnabled;

  tpSubHeader.classList.toggle("tp-off", !trackingEnabled);
}

document.addEventListener("DOMContentLoaded", function() {
  if (!RPMIsWindowPrivate()) {
    document.documentElement.classList.remove("private");
    document.documentElement.classList.add("normal");
    document.getElementById("favicon").setAttribute("href", FAVICON_QUESTION);
    document.getElementById("startPrivateBrowsing").addEventListener("click", function() {
      RPMSendAsyncMessage("OpenPrivateWindow");
    });
    return;
  }

  contentBlockingUIEnabled = RPMGetBoolPref(CB_UI_ENABLED_PREF);

  document.getElementById("startTour").addEventListener("click", function() {
    RPMSendAsyncMessage("DontShowIntroPanelAgain");
  });

  let introURL = RPMGetFormatURLPref("privacy.trackingprotection.introURL");
  // If the CB UI is enabled, tell the tour page to show a different variation
  // that is updated to reflect the CB control center UI.
  let variation = contentBlockingUIEnabled ? "?variation=1" : "";

  document.getElementById("startTour").setAttribute("href", introURL + variation);

  document.getElementById("learnMore").setAttribute("href",
    RPMGetFormatURLPref("app.support.baseURL") + "private-browsing");

  let tpToggle = document.getElementById("tpToggle");
  document.getElementById("tpButton").addEventListener("click", () => {
    tpToggle.click();
  });
  tpToggle.addEventListener("change", async function() {
    let promises = [];
    if (tpToggle.checked && contentBlockingUIEnabled) {
      promises.push(RPMSetBoolPref(CB_ENABLED_PREF, true));
    }

    promises.push(RPMSetBoolPref(TP_PB_ENABLED_PREF, tpToggle.checked));

    await Promise.all(promises);

    updateTPInfo();
  });

  updateTPInfo();
});
