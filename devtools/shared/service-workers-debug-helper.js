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
 */

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
  canDebugServiceWorkers,
  isParentInterceptEnabled,
};
