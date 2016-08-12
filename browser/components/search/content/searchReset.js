/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

const TELEMETRY_RESULT_ENUM = {
  RESTORED_DEFAULT: 0,
  KEPT_CURRENT: 1,
  CHANGED_ENGINE: 2,
  CLOSED_PAGE: 3,
  OPENED_SETTINGS: 4
};

window.onload = function() {
  let defaultEngine = document.getElementById("defaultEngine");
  let originalDefault = Services.search.originalDefaultEngine;
  defaultEngine.textContent = originalDefault.name;
  defaultEngine.style.backgroundImage =
    'url("' + originalDefault.iconURI.spec + '")';

  document.getElementById("searchResetChangeEngine").focus();
  window.addEventListener("unload", recordPageClosed);
  document.getElementById("linkSettingsPage")
          .addEventListener("click", openingSettings);
};

function doSearch() {
  let queryString = "";
  let purpose = "";
  let params = window.location.href.match(/^about:searchreset\?([^#]*)/);
  if (params) {
    params = params[1].split("&");
    for (let param of params) {
      if (param.startsWith("data="))
        queryString = decodeURIComponent(param.slice(5));
      else if (param.startsWith("purpose="))
        purpose = param.slice(8);
    }
  }

  let engine = Services.search.currentEngine;
  let submission = engine.getSubmission(queryString, null, purpose);

  window.removeEventListener("unload", recordPageClosed);

  let win = window.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIWebNavigation)
                  .QueryInterface(Ci.nsIDocShellTreeItem)
                  .rootTreeItem
                  .QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDOMWindow);
  win.openUILinkIn(submission.uri.spec, "current", false, submission.postData);
}

function openingSettings() {
  record(TELEMETRY_RESULT_ENUM.OPENED_SETTINGS);
  window.removeEventListener("unload", recordPageClosed);
}

function record(result) {
  Services.telemetry.getHistogramById("SEARCH_RESET_RESULT").add(result);
}

function keepCurrentEngine() {
  // Calling the currentEngine setter will force a correct loadPathHash to be
  // written for this engine, so that we don't prompt the user again.
  Services.search.currentEngine = Services.search.currentEngine;
  record(TELEMETRY_RESULT_ENUM.KEPT_CURRENT);
  doSearch();
}

function changeSearchEngine() {
  let engine = Services.search.originalDefaultEngine;
  if (engine.hidden)
    engine.hidden = false;
  Services.search.currentEngine = engine;

  record(TELEMETRY_RESULT_ENUM.RESTORED_DEFAULT);

  doSearch();
}

function recordPageClosed() {
  record(TELEMETRY_RESULT_ENUM.CLOSED_PAGE);
}
