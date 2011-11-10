/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Firefox Sync.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Allison Naaktgeboren <ally@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
