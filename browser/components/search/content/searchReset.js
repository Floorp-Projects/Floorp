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
  CLOSED_PAGE: 3
};

window.onload = function() {
  let list = document.getElementById("defaultEngine");
  let originalDefault = Services.search.originalDefaultEngine.name;
  Services.search.getDefaultEngines().forEach(e => {
    let opt = document.createElement("option");
    opt.setAttribute("value", e.name);
    opt.engine = e;
    opt.textContent = e.name;
    if (e.iconURI)
      opt.style.backgroundImage = 'url("' + e.iconURI.spec + '")';
    if (e.name == originalDefault)
      opt.setAttribute("selected", "true");
    list.appendChild(opt);
  });

  let updateIcon = () => {
    list.style.setProperty("--engine-icon-url",
                           list.selectedOptions[0].style.backgroundImage);
  };

  list.addEventListener("change", updateIcon);
  // When selecting using the keyboard, the 'change' event is only fired after
  // the user presses <enter> or moves the focus elsewhere.
  // keypress/keyup fire too late and cause flicker when updating the icon.
  // keydown fires too early and the selected option isn't changed yet.
  list.addEventListener("keydown", () => {
    Services.tm.mainThread.dispatch(updateIcon, Ci.nsIThread.DISPATCH_NORMAL);
  });
  updateIcon();

  document.getElementById("searchResetChangeEngine").focus();
  window.addEventListener("unload", recordPageClosed);
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
  let list = document.getElementById("defaultEngine");
  let engine = list.selectedOptions[0].engine;
  if (engine.hidden)
    engine.hidden = false;
  Services.search.currentEngine = engine;

  // Record if we restored the original default or changed to another engine.
  let originalDefault = Services.search.originalDefaultEngine.name;
  let code = TELEMETRY_RESULT_ENUM.CHANGED_ENGINE;
  if (Services.search.originalDefaultEngine.name == engine.name)
    code = TELEMETRY_RESULT_ENUM.RESTORED_DEFAULT;
  record(code);

  doSearch();
}

function recordPageClosed() {
  record(TELEMETRY_RESULT_ENUM.CLOSED_PAGE);
}
