/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-unused-vars */

"use strict";

const nsLoginInfo = new Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  Ci.nsILoginInfo,
  "init"
);

const TEST_LOGIN1 = new nsLoginInfo(
  "https://example.com/",
  "https://example.com/",
  null,
  "user1",
  "pass1",
  "username",
  "password"
);

const TEST_LOGIN2 = new nsLoginInfo(
  "https://2.example.com/",
  "https://2.example.com/",
  null,
  "user2",
  "pass2",
  "username",
  "password"
);

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
        numLogins: Services.logins.countLogins("", "", ""),
        mobileDeviceConnected,
      };
    },
  };
};

// Used to replace AboutProtectionsHandler.getMonitorData in front-end tests.
const mockGetMonitorData = (potentiallyBreachedLogins = 0, error = false) => {
  return {
    getMonitorData: () => {
      if (error) {
        return { error };
      }

      return {
        monitoredEmails: 1,
        numBreaches: 11,
        passwords: 8,
        potentiallyBreachedLogins,
        error,
      };
    },
  };
};
