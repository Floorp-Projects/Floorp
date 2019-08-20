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
 * This helper provides info on whether we can debug service workers or not.
 * This depends both on the current multiprocess (e10s) configuration and on the
 * usage of the new service worker implementation
 * (dom.serviceWorkers.parent_intercept = true).
 *
 * Since this can be changed on the fly, there are subscribe/unsubscribe functions
 * to get notified of this.
 */

const PROCESS_COUNT_PREF = "dom.ipc.processCount";
const MULTI_OPTOUT_PREF = "dom.ipc.multiOptOut";

function addDebugServiceWorkersListener(listener) {
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

function removeDebugServiceWorkersListener(listener) {
  Services.prefs.removeObserver(PROCESS_COUNT_PREF, listener);
  Services.prefs.removeObserver(MULTI_OPTOUT_PREF, listener);
}

function canDebugServiceWorkers() {
  const isE10s = Services.appinfo.browserTabsRemoteAutostart;
  const processCount = Services.appinfo.maxWebProcessCount;
  const multiE10s = isE10s && processCount > 1;
  const isNewSWImplementation = swm.isParentInterceptEnabled();

  // When can we debug Service Workers?
  // If we're running the new implementation, OR if not in multiprocess mode
  return isNewSWImplementation || !multiE10s;
}

function isParentInterceptEnabled() {
  return swm.isParentInterceptEnabled();
}

module.exports = {
  addDebugServiceWorkersListener,
  canDebugServiceWorkers,
  isParentInterceptEnabled,
  removeDebugServiceWorkersListener,
};
