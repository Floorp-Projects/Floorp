/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://services-common/preferences.js");

const reporter = Cc["@mozilla.org/datareporting/service;1"]
                   .getService(Ci.nsISupports)
                   .wrappedJSObject
                   .healthReporter;

const policy = Cc["@mozilla.org/datareporting/service;1"]
                 .getService(Ci.nsISupports)
                 .wrappedJSObject
                 .policy;

const prefs = new Preferences("datareporting.healthreport.about.");

function getLocale() {
   return Cc["@mozilla.org/chrome/chrome-registry;1"]
            .getService(Ci.nsIXULChromeRegistry)
            .getSelectedLocale("global");
}

function init() {
  refreshWithDataSubmissionFlag(policy.healthReportUploadEnabled);
  refreshJSONPayload();
  document.getElementById("details-link").href = prefs.get("glossaryUrl");
}

/**
 * Update the state of the page to reflect the current data submission state.
 *
 * @param enabled
 *        (bool) Whether data submission is enabled.
 */
function refreshWithDataSubmissionFlag(enabled) {
  if (!enabled) {
    updateView("disabled");
  } else {
    updateView("default");
  }
}

function updateView(state="default") {
  let content = document.getElementById("content");
  let controlContainer = document.getElementById("control-container");
  content.setAttribute("state", state);
  controlContainer.setAttribute("state", state);
}

function refreshDataView(data) {
  let noData = document.getElementById("data-no-data");
  let dataEl = document.getElementById("raw-data");

  noData.style.display = data ? "none" : "inline";
  dataEl.style.display = data ? "block" : "none";
  if (data) {
    dataEl.innerHTML = JSON.stringify(data, null, 2);
  }
}

/**
 * Ensure the page has the latest version of the uploaded JSON payload.
 */
function refreshJSONPayload() {
  reporter.getLastPayload().then(refreshDataView);
}

function onOptInClick() {
  policy.recordHealthReportUploadEnabled(true,
                                         "Clicked opt in button on about page.");
  refreshWithDataSubmissionFlag(true);
}

function onOptOutClick() {
  let prompts = Cc["@mozilla.org/embedcomp/prompt-service;1"]
                  .getService(Ci.nsIPromptService);

  let messages = document.getElementById("optout-confirmationPrompt");
  let title = messages.getAttribute("confirmationPrompt_title");
  let message = messages.getAttribute("confirmationPrompt_message");

  if (!prompts.confirm(window, title, message)) {
    return;
  }

  policy.recordHealthReportUploadEnabled(false,
                                         "Clicked opt out button on about page.");
  refreshWithDataSubmissionFlag(false);
  updateView("disabled");
}

function onShowRawDataClick() {
  updateView("showDetails");
  refreshJSONPayload();
}

function onHideRawDataClick() {
  updateView("default");
}

function onShowReportClick() {
  updateView("showReport");
  document.getElementById("remote-report").src = prefs.get("reportUrl");
}

function onHideReportClick() {
  updateView("default");
  document.getElementById("remote-report").src = "";
}

