/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-sync/main.js");

let gProgressBar;
let gCounter = 0;

function onLoad(event) {
  Services.obs.addObserver(onEngineSync, "weave:engine:sync:finish", false);
  Services.obs.addObserver(onEngineSync, "weave:engine:sync:error", false);
  Services.obs.addObserver(onServiceSync, "weave:service:sync:finish", false);
  Services.obs.addObserver(onServiceSync, "weave:service:sync:error", false);

  gProgressBar = document.getElementById('uploadProgressBar');

  if (Services.prefs.getPrefType("services.sync.firstSync") != Ci.nsIPrefBranch.PREF_INVALID) {
    gProgressBar.style.display = "inline";
  }
  else {
    gProgressBar.style.display = "none";
  }
}

function onUnload(event) {
  cleanUpObservers();
}

function cleanUpObservers() {
  try {
    Services.obs.removeObserver(onEngineSync, "weave:engine:sync:finish", false);
    Services.obs.removeObserver(onEngineSync, "weave:engine:sync:error", false);
    Services.obs.removeObserver(onServiceSync, "weave:service:sync:finish", false);
    Services.obs.removeObserver(onServiceSync, "weave:service:sync:error", false);
  }
  catch (e) {
    // may be double called by unload & exit. Ignore.
  }
}

function onEngineSync(subject, topic, data) {
  // The Clients engine syncs first. At this point we don't necessarily know
  // yet how many engines will be enabled, so we'll ignore the Clients engine
  // and evaluate how many engines are enabled when the first "real" engine
  // syncs.
  if (data == "clients") {
    return;
  }

  if (!gCounter &&
      Services.prefs.getPrefType("services.sync.firstSync") != Ci.nsIPrefBranch.PREF_INVALID) {
    gProgressBar.max = Weave.Engines.getEnabled().length;
  }

  gCounter += 1;
  gProgressBar.setAttribute("value", gCounter);
}

function onServiceSync(subject, topic, data) {
  // To address the case where 0 engines are synced, we will fill the
  // progress bar so the user knows that the sync has finished.
  gProgressBar.setAttribute("value", gProgressBar.max);
  cleanUpObservers();
}

function closeTab() {
  window.close();
}
