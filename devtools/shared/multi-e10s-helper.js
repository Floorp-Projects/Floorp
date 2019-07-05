/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "swm",
  "@mozilla.org/serviceworkers/manager;1",
  "nsIServiceWorkerManager"
);

/**
 * This helper provides info on whether we are in multi e10s mode or not.
 * Since this can be changed on the fly, there are subscribe/unsubscribe functions
 * to get notified of this.
 *
 * The logic to handle this is borrowed from the (old) about:debugging code.
 */

const PROCESS_COUNT_PREF = "dom.ipc.processCount";
const MULTI_OPTOUT_PREF = "dom.ipc.multiOptOut";

function addMultiE10sListener(listener) {
  // Some notes about these observers:
  // - nsIPrefBranch.addObserver observes prefixes. In reality, watching
  //   PROCESS_COUNT_PREF watches two separate prefs:
  //   dom.ipc.processCount *and* dom.ipc.processCount.web. Because these
  //   are the two ways that we control the number of content processes,
  //   that works perfectly fine.
  // - The user might opt in or out of multi by setting the multi opt out
  //   pref. That affects whether we need to show our warning, so we need to
  //   update our state when that pref changes.
  // - In all cases, we don't have to manually check which pref changed to
  //   what. The platform code in nsIXULRuntime.maxWebProcessCount does all
  //   of that for us.
  Services.prefs.addObserver(PROCESS_COUNT_PREF, listener);
  Services.prefs.addObserver(MULTI_OPTOUT_PREF, listener);
}

function removeMultiE10sListener(listener) {
  Services.prefs.removeObserver(PROCESS_COUNT_PREF, listener);
  Services.prefs.removeObserver(MULTI_OPTOUT_PREF, listener);
}

// TODO to be refactored as `canDebugServiceWorkers` (and
// logic in the consumers of the function to be changed)
// See Bug 1531349
function isMultiE10s() {
  const isE10s = Services.appinfo.browserTabsRemoteAutostart;
  const processCount = Services.appinfo.maxWebProcessCount;
  const multiE10s = isE10s && processCount > 1;
  const isNewSWImplementation = swm.isParentInterceptEnabled();

  // When can we debug Service Workers?
  // If we're running the new implementation, OR if not in multiprocess mode
  const canDebugSW = isNewSWImplementation || !multiE10s;
  return !canDebugSW;
}

module.exports = {
  addMultiE10sListener,
  isMultiE10s,
  removeMultiE10sListener,
};
