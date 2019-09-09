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

let fakeDataWithNoError = {
  monitoredEmails: 1,
  numBreaches: 11,
  passwords: 8,
  potentiallyBreachedLogins: 2,
  error: false,
};

let fakeDataWithError = {
  monitoredEmails: null,
  numBreaches: null,
  passwords: null,
  potentiallyBreachedLogins: null,
  error: true,
};

// Modify AboutProtectionsHandler's getMonitorData method to fake returning data from the
// Monitor endpoint.
const mockGetMonitorData = data => async () => data;

// Modify AboutProtectionsHandler's getLoginData method to fake being logged in with Fxa.
const mockGetLoginData = async () => {
  return {
    hasFxa: true,
    numLogins: Services.logins.countLogins("", "", ""),
    numSyncedDevices: 0,
  };
};

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  const { getLoginData } = AboutProtectionsHandler;
  const { getMonitorData } = AboutProtectionsHandler;

  await reloadTab(tab);

  info("Check that the correct content is displayed for users with no logins.");
  await checkNoLoginsContentIsDisplayed(tab, "monitor-sign-up");

  info(
    "Check that the correct content is displayed for users with monitor data."
  );
  Services.logins.addLogin(TEST_LOGIN1);
  AboutProtectionsHandler.getMonitorData = mockGetMonitorData(
    fakeDataWithNoError
  );
  await reloadTab(tab);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    await ContentTaskUtils.waitForCondition(() => {
      const hasLogins = content.document.querySelector(
        ".monitor-card.has-logins"
      );
      return hasLogins && ContentTaskUtils.is_visible(hasLogins);
    }, "Monitor card for user with stored logins is shown.");

    const hasLoginsHeaderContent = content.document.querySelector(
      "#monitor-header-content span"
    );
    const cardBody = content.document.querySelector(".monitor-card .card-body");

    ok(
      ContentTaskUtils.is_visible(cardBody),
      "Card body is shown for users monitor data."
    );
    await ContentTaskUtils.waitForCondition(() => {
      return (
        hasLoginsHeaderContent.textContent ==
        "Firefox Monitor warns you if your info has appeared in a known data breach."
      );
    }, "Header content for user with monitor data is correct.");

    info("Make sure correct numbers for monitor stats are displayed.");
    const emails = content.document.querySelector(
      ".monitor-stat span[data-type='stored-emails']"
    );
    const passwords = content.document.querySelector(
      ".monitor-stat span[data-type='exposed-passwords']"
    );
    const breaches = content.document.querySelector(
      ".monitor-stat span[data-type='known-breaches']"
    );
    const breachedLockwisePasswords = content.document.querySelector(
      ".monitor-breached-passwords span[data-type='breached-lockwise-passwords']"
    );

    is(emails.textContent, 1, "1 monitored email is displayed");
    is(passwords.textContent, 8, "8 exposed passwords are displayed");
    is(breaches.textContent, 11, "11 known data breaches are displayed.");
    is(
      breachedLockwisePasswords.textContent,
      2,
      "2 saved passwords are displayed."
    );
  });

  info("Make sure Lockwise section is hidden if no passwords are returned.");
  const noBreachedLoginsData = {
    ...fakeDataWithNoError,
    potentiallyBreachedLogins: 0,
  };
  AboutProtectionsHandler.getMonitorData = mockGetMonitorData(
    noBreachedLoginsData
  );

  await reloadTab(tab);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    await ContentTaskUtils.waitForCondition(() => {
      const hasLogins = content.document.querySelector(
        ".monitor-card.has-logins"
      );
      return hasLogins && ContentTaskUtils.is_visible(hasLogins);
    }, "Monitor card for user with stored logins is shown.");

    const lockwiseSection = content.document.querySelector(
      ".monitor-breached-passwords"
    );

    ok(
      ContentTaskUtils.is_hidden(lockwiseSection),
      "Lockwise section is hidden."
    );
  });

  info(
    "Check that correct content is displayed when monitor data contains an error message."
  );
  AboutProtectionsHandler.getMonitorData = mockGetMonitorData(
    fakeDataWithError
  );
  AboutProtectionsHandler.getLoginData = mockGetLoginData;
  await reloadTab(tab);
  await checkNoLoginsContentIsDisplayed(tab);

  info("Disable showing the Monitor card.");
  Services.prefs.setBoolPref(
    "browser.contentblocking.report.monitor.enabled",
    false
  );
  await reloadTab(tab);

  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    await ContentTaskUtils.waitForCondition(() => {
      const monitorCard = content.document.querySelector(".monitor-card");
      return !monitorCard["data-enabled"];
    }, "Monitor card is not enabled.");

    const monitorCard = content.document.querySelector(".monitor-card");
    ok(ContentTaskUtils.is_hidden(monitorCard), "Monitor card is hidden.");
  });

  // set the pref back to displaying the card.
  Services.prefs.setBoolPref(
    "browser.contentblocking.report.monitor.enabled",
    true
  );

  // remove logins
  Services.logins.removeLogin(TEST_LOGIN1);

  // restore original getLoginData & getMonitorData methods to AboutProtectionsHandler
  AboutProtectionsHandler.getLoginData = getLoginData;
  AboutProtectionsHandler.getMonitorData = getMonitorData;

  await BrowserTestUtils.removeTab(tab);
});

async function checkNoLoginsContentIsDisplayed(tab, expectedLinkContent) {
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    await ContentTaskUtils.waitForCondition(() => {
      const noLogins = content.document.querySelector(
        ".monitor-card.no-logins"
      );
      return noLogins && ContentTaskUtils.is_visible(noLogins);
    }, "Monitor card for user with no logins is shown.");

    const noLoginsHeaderContent = content.document.querySelector(
      "#monitor-header-content span"
    );
    const cardBody = content.document.querySelector(".monitor-card .card-body");

    ok(
      ContentTaskUtils.is_hidden(cardBody),
      "Card body is hidden for users with no logins."
    );
    is(
      noLoginsHeaderContent.getAttribute("data-l10n-id"),
      "monitor-header-content-no-account",
      "Header content for user with no logins is correct"
    );
  });
}
