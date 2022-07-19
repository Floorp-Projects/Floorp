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

XPCOMUtils.defineLazyModuleGetters(this, {
  Region: "resource://gre/modules/Region.jsm",
});

const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
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

// Used to replace AboutProtectionsHandler.getLoginData in front-end tests.
const mockGetLoginDataWithSyncedDevices = (
  mobileDeviceConnected = false,
  potentiallyBreachedLogins = 0
) => {
  return {
    getLoginData: () => {
      return {
        numLogins: Services.logins.countLogins("", "", ""),
        potentiallyBreachedLogins,
        mobileDeviceConnected,
      };
    },
  };
};

// Used to replace AboutProtectionsHandler.getMonitorData in front-end tests.
const mockGetMonitorData = data => {
  return {
    getMonitorData: () => {
      if (data.error) {
        return data;
      }

      return {
        monitoredEmails: 1,
        numBreaches: data.numBreaches,
        passwords: 8,
        numBreachesResolved: data.numBreachesResolved,
        passwordsResolved: 1,
        error: false,
      };
    },
  };
};

registerCleanupFunction(function head_cleanup() {
  Services.logins.removeAllUserFacingLogins();
});

// Used to replace AboutProtectionsParent.VPNSubStatus
const getVPNOverrides = (hasSubscription = false) => {
  return {
    vpnOverrides: () => {
      return hasSubscription;
    },
  };
};

const promiseSetHomeRegion = async region => {
  let promise = SearchTestUtils.promiseSearchNotification("engines-reloaded");
  Region._setHomeRegion(region);
  await promise;
};
