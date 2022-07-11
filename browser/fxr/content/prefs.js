/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from common.js */

var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const PREF_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

window.addEventListener(
  "DOMContentLoaded",
  () => {
    initAboutInfo();
    initClearAllData();
    initSubmitHealthReport();
    initParentDependencies();
  },
  { once: true }
);

function initAboutInfo() {
  // Bug 1586294 - Update FxR Desktop Settings to use Fluent
  document.getElementById("eFxrVersion").textContent = "version 0.9";
  document.getElementById("eFxrDate").textContent = "(2019-12-17)";
  document.getElementById("eFxVersion").textContent =
    "Firefox version " + Services.appinfo.version;
}

function initClearAllData() {
  let eClearPrompt = document.getElementById("eClearPrompt");

  let eClearTry = document.getElementById("eClearTry");
  eClearTry.addEventListener("click", () => {
    showModalContainer(eClearPrompt);
  });

  // Note: the calls to clearModalContainer below return eClearPrompt
  // back to <body> to be reused later, because it is moved into anothe
  // element in showModalContainer.

  let eClearCancel = document.getElementById("eClearCancel");
  eClearCancel.addEventListener("click", () => {
    document.body.appendChild(clearModalContainer());
  });

  // When the confirm option is visible, do the work to actually clear the data
  document.getElementById("eClearConfirm").addEventListener("click", () => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, function(
      aFailedFlags
    ) {
      if (aFailedFlags == 0) {
        eClearTry.textContent = "Data cleared";
        eClearTry.disabled = true;
        document.body.appendChild(clearModalContainer());
      } else {
        eClearTry.textContent = "Error";
      }
    });
  });
}

// Based on https://searchfox.org/mozilla-central/source/browser/components/preferences/privacy.js
function initSubmitHealthReport() {
  let checkbox = document.getElementById("eCrashConfig");

  // Telemetry is only sending data if MOZ_TELEMETRY_REPORTING is defined.
  // We still want to display the preferences panel if that's not the case, but
  // we want it to be disabled and unchecked.
  if (
    Services.prefs.prefIsLocked(PREF_UPLOAD_ENABLED) ||
    !AppConstants.MOZ_TELEMETRY_REPORTING
  ) {
    checkbox.disabled = true;
  } else {
    checkbox.addEventListener("change", updateSubmitHealthReport);

    checkbox.checked =
      Services.prefs.getBoolPref(PREF_UPLOAD_ENABLED) &&
      AppConstants.MOZ_TELEMETRY_REPORTING;
  }
}

/**
 * Update the health report preference with state from checkbox.
 */
function updateSubmitHealthReport() {
  let checkbox = document.getElementById("eCrashConfig");
  Services.prefs.setBoolPref(PREF_UPLOAD_ENABLED, checkbox.checked);
}

// prefs.html can be loaded as another <browser> from fxrui.html. In this
// scenario, some actions are propogated to the parent
function initParentDependencies() {
  if (window.parent != window) {
    // Close the <browser> instance that loaded this page
    document.getElementById("eCloseSettings").addEventListener("click", () => {
      window.parent.closeSettings();
    });

    // Load the relevant URLs into the top UI's <browser>
    document.getElementById("ePrivacyPolicy").addEventListener("click", () => {
      window.parent.showPrivacyPolicy();
    });

    document.getElementById("eLicenseInfo").addEventListener("click", () => {
      window.parent.showLicenseInfo();
    });

    document.getElementById("eReportIssue").addEventListener("click", () => {
      window.parent.showReportIssue();
    });
  }
}
