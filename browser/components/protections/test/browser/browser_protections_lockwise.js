/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AboutProtectionsHandler } = ChromeUtils.import(
  "resource:///modules/aboutpages/AboutProtectionsHandler.jsm"
);

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

// Modify AboutProtectionsHandler's getLoginData method to fake returning a specified
// number of devices.
const mockGetLoginDataWithSyncedDevices = deviceCount => async () => {
  const loginCount = Services.logins.countLogins("", "", "");

  return {
    isLoggedIn: loginCount > 0 || deviceCount > 0,
    numberOfLogins: loginCount,
    numberOfSyncedDevices: deviceCount,
  };
};

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  const { getLoginData } = AboutProtectionsHandler;

  info("Check that the correct content is displayed for non-logged in users.");
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    await ContentTaskUtils.waitForCondition(() => {
      const noLogins = content.document.querySelector(".no-logins");
      return ContentTaskUtils.is_visible(noLogins);
    }, "Lockwise card for user with no logins is shown.");

    const noLoginsContent = content.document.querySelector(
      "#lockwise-body-content .no-logins"
    );
    const hasLoginsContent = content.document.querySelector(
      "#lockwise-body-content .has-logins"
    );

    ok(
      ContentTaskUtils.is_visible(noLoginsContent),
      "Content for user with no logins is shown."
    );
    ok(
      ContentTaskUtils.is_hidden(hasLoginsContent),
      "Content for user with logins is hidden."
    );
  });

  info("Add a login and check that content for a logged in user is displayed.");
  Services.logins.addLogin(TEST_LOGIN1);
  await reloadTab(tab);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    await ContentTaskUtils.waitForCondition(() => {
      const hasLogins = content.document.querySelector(".has-logins");
      return ContentTaskUtils.is_visible(hasLogins);
    }, "Lockwise card for user with logins is shown.");

    const noLoginsContent = content.document.querySelector(
      "#lockwise-body-content .no-logins"
    );
    const hasLoginsContent = content.document.querySelector(
      "#lockwise-body-content .has-logins"
    );
    const numberOfLogins = hasLoginsContent.querySelector(
      ".number-of-logins.block"
    );
    const numberOfSyncedDevices = hasLoginsContent.querySelector(
      ".number-of-synced-devices.block"
    );
    const syncedDevicesStatusText = content.document.querySelector(
      ".synced-devices-text span"
    );

    ok(
      ContentTaskUtils.is_hidden(noLoginsContent),
      "Content for user with no logins is hidden."
    );
    ok(
      ContentTaskUtils.is_visible(hasLoginsContent),
      "Content for user with logins is shown."
    );
    is(numberOfLogins.textContent, 1, "One stored login should be displayed");

    info("Also check that content for no synced devices is correct.");
    is(
      numberOfSyncedDevices.textContent,
      0,
      "Zero synced devices are displayed."
    );
    is(syncedDevicesStatusText.textContent, "Not syncing to other devices.");
  });

  info(
    "Add another login and check the number of stored logins is updated after reload."
  );
  Services.logins.addLogin(TEST_LOGIN2);
  await reloadTab(tab);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    await ContentTaskUtils.waitForCondition(() => {
      const hasLogins = content.document.querySelector(".has-logins");
      return ContentTaskUtils.is_visible(hasLogins);
    }, "Lockwise card for user with logins is shown.");

    const numberOfLogins = content.document.querySelector(
      "#lockwise-body-content .has-logins .number-of-logins.block"
    );

    is(numberOfLogins.textContent, 2, "Two stored logins should be displayed");
  });

  info(
    "Mock login data with synced devices and check that the correct number and content is displayed."
  );
  AboutProtectionsHandler.getLoginData = mockGetLoginDataWithSyncedDevices(5);
  await reloadTab(tab);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    await ContentTaskUtils.waitForCondition(() => {
      const hasLogins = content.document.querySelector(".has-logins");
      return ContentTaskUtils.is_visible(hasLogins);
    }, "Lockwise card for user with logins is shown.");

    const numberOfSyncedDevices = content.document.querySelector(
      ".number-of-synced-devices.block"
    );
    const syncedDevicesStatusText = content.document.querySelector(
      ".synced-devices-text span"
    );

    is(
      numberOfSyncedDevices.textContent,
      5,
      "Five synced devices should be displayed"
    );
    is(syncedDevicesStatusText.textContent, "Syncing to 5 other devices.");
  });

  info("Disable showing the Lockwise card.");
  Services.prefs.setBoolPref(
    "browser.contentblocking.report.lockwise.enabled",
    false
  );
  await reloadTab(tab);
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    await ContentTaskUtils.waitForCondition(() => {
      const lockwiseCard = content.document.querySelector(".lockwise-card");
      return !lockwiseCard["data-enabled"];
    }, "Lockwise card is not enabled.");

    const lockwiseCard = content.document.querySelector(".lockwise-card");
    ok(ContentTaskUtils.is_hidden(lockwiseCard), "Lockwise card is hidden.");
  });

  // set the pref back to displaying the card.
  Services.prefs.setBoolPref(
    "browser.contentblocking.report.lockwise.enabled",
    true
  );

  // remove logins
  Services.logins.removeLogin(TEST_LOGIN1);
  Services.logins.removeLogin(TEST_LOGIN2);

  AboutProtectionsHandler.getLoginData = getLoginData;
  await BrowserTestUtils.removeTab(tab);
});
