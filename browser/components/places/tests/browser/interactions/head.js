/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Interactions } = ChromeUtils.import(
  "resource:///modules/Interactions.jsm"
);

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "pageViewIdleTime",
  "browser.places.interactions.pageViewIdleTime",
  60
);

/**
 * Register the mock idleSerice.
 *
 * @param {Fun} registerCleanupFunction
 */
function disableIdleService() {
  let idleService = Cc["@mozilla.org/widget/useridleservice;1"].getService(
    Ci.nsIUserIdleService
  );
  idleService.removeIdleObserver(Interactions, pageViewIdleTime);

  registerCleanupFunction(() => {
    idleService.addIdleObserver(Interactions, pageViewIdleTime);
  });
}
