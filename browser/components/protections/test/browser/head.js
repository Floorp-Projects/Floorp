/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-unused-vars */

"use strict";

async function reloadTab(tab) {
  const tabReloaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.reloadTab(tab);
  await tabReloaded;
}

// Used to replace AboutProtectionsHandler.getLoginData in front-end tests.
const mockGetLoginDataWithSyncedDevices = (mobileDeviceConnected = false) => {
  return {
    getLoginData: () => {
      return {
        hasFxa: true,
        numLogins: Services.logins.countLogins("", "", ""),
        mobileDeviceConnected,
      };
    },
  };
};
