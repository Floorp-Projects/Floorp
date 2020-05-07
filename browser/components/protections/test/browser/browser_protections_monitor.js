/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AboutProtectionsParent } = ChromeUtils.import(
  "resource:///actors/AboutProtectionsParent.jsm"
);

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  await reloadTab(tab);

  info("Check that the correct content is displayed for users with no logins.");
  await checkNoLoginsContentIsDisplayed(tab, "monitor-sign-up");

  info(
    "Check that the correct content is displayed for users with monitor data."
  );
  Services.logins.addLogin(TEST_LOGIN1);
  AboutProtectionsParent.setTestOverride(mockGetMonitorData());
  await reloadTab(tab);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
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

    is(emails.textContent, 1, "1 monitored email is displayed");
    is(passwords.textContent, 8, "8 exposed passwords are displayed");
    is(breaches.textContent, 11, "11 known data breaches are displayed.");
  });

  info(
    "Check that correct content is displayed when monitor data contains an error message."
  );
  AboutProtectionsParent.setTestOverride(mockGetMonitorData(0, true));
  await reloadTab(tab);
  await checkNoLoginsContentIsDisplayed(tab);

  info("Disable showing the Monitor card.");
  Services.prefs.setBoolPref(
    "browser.contentblocking.report.monitor.enabled",
    false
  );
  await reloadTab(tab);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
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

  // restore original test functions
  AboutProtectionsParent.setTestOverride(null);

  await BrowserTestUtils.removeTab(tab);
});

async function checkNoLoginsContentIsDisplayed(tab, expectedLinkContent) {
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
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
