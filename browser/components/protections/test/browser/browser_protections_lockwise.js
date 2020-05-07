/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AboutProtectionsParent } = ChromeUtils.import(
  "resource:///actors/AboutProtectionsParent.jsm"
);
const ABOUT_LOGINS_URL = "about:logins";

add_task(async function testNoLoginsLockwiseCardUI() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  let aboutLoginsPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    ABOUT_LOGINS_URL
  );

  info(
    "Check that the correct lockwise card content is displayed for non-logged in users."
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    await ContentTaskUtils.waitForCondition(() => {
      const lockwiseCard = content.document.querySelector(".lockwise-card");
      return ContentTaskUtils.is_visible(lockwiseCard);
    }, "Lockwise card for user with no logins is visible.");

    const lockwiseTitle = content.document.querySelector("#lockwise-title");
    is(
      lockwiseTitle.textContent,
      "Never forget a password again",
      "Correct lockwise title is shown"
    );

    const lockwiseHowItWorks = content.document.querySelector(
      "#lockwise-how-it-works"
    );
    ok(
      ContentTaskUtils.is_hidden(lockwiseHowItWorks),
      "How it works link is hidden"
    );

    const lockwiseHeaderString = content.document.querySelector(
      "#lockwise-header-content span"
    ).textContent;
    ok(
      lockwiseHeaderString.includes(
        "Firefox Lockwise securely stores your passwords in your browser"
      ),
      "Correct lockwise header string is shown"
    );

    const lockwiseScannedWrapper = content.document.querySelector(
      ".lockwise-scanned-wrapper"
    );
    ok(
      ContentTaskUtils.is_hidden(lockwiseScannedWrapper),
      "Lockwise scanned wrapper is hidden"
    );

    const lockwiseBodyContent = content.document.querySelector(
      "#lockwise-body-content .no-logins"
    );
    ok(
      ContentTaskUtils.is_visible(lockwiseBodyContent),
      "Lockwise app content is visible"
    );

    const managePasswordsButton = content.document.querySelector(
      "#manage-passwords-button"
    );
    ok(
      ContentTaskUtils.is_hidden(managePasswordsButton),
      "Manage passwords button is hidden"
    );

    const savePasswordsButton = content.document.querySelector(
      "#save-passwords-button"
    );
    ok(
      ContentTaskUtils.is_visible(savePasswordsButton),
      "Save passwords button is visible in the header"
    );
    info(
      "Click on the save passwords button and check that it opens about:logins in a new tab"
    );
    savePasswordsButton.click();
  });
  let loginsTab = await aboutLoginsPromise;
  info("about:logins was successfully opened in a new tab");
  gBrowser.removeTab(loginsTab);
  gBrowser.removeTab(tab);
});

add_task(async function testLockwiseCardUIWithLogins() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  let aboutLoginsPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    ABOUT_LOGINS_URL
  );

  info(
    "Add a login and check that lockwise card content for a logged in user is displayed correctly"
  );
  Services.logins.addLogin(TEST_LOGIN1);
  await reloadTab(tab);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    await ContentTaskUtils.waitForCondition(() => {
      const hasLogins = content.document.querySelector(".lockwise-card");
      return ContentTaskUtils.is_visible(hasLogins);
    }, "Lockwise card for user with logins is visible");

    const lockwiseTitle = content.document.querySelector("#lockwise-title");
    is(
      lockwiseTitle.textContent,
      "Password Management",
      "Correct lockwise title is shown"
    );

    const lockwiseHowItWorks = content.document.querySelector(
      "#lockwise-how-it-works"
    );
    ok(
      ContentTaskUtils.is_visible(lockwiseHowItWorks),
      "How it works link is visible"
    );

    const lockwiseHeaderString = content.document.querySelector(
      "#lockwise-header-content span"
    ).textContent;
    ok(
      lockwiseHeaderString.includes(
        "Securely store and sync your passwords to all your devices"
      ),
      "Correct lockwise header string is shown"
    );

    const lockwiseScannedWrapper = content.document.querySelector(
      ".lockwise-scanned-wrapper"
    );
    ok(
      ContentTaskUtils.is_visible(lockwiseScannedWrapper),
      "Lockwise scanned wrapper is visible"
    );

    const lockwiseScannedText = content.document.querySelector(
      "#lockwise-scanned-text"
    ).textContent;
    is(
      lockwiseScannedText,
      "1 password stored securely.",
      "Correct lockwise scanned text is shown"
    );

    const lockwiseBodyContent = content.document.querySelector(
      "#lockwise-body-content .no-logins"
    );
    ok(
      ContentTaskUtils.is_hidden(lockwiseBodyContent),
      "Lockwise app content is hidden"
    );

    const savePasswordsButton = content.document.querySelector(
      "#save-passwords-button"
    );
    ok(
      ContentTaskUtils.is_hidden(savePasswordsButton),
      "Save passwords button is hidden"
    );

    const managePasswordsButton = content.document.querySelector(
      "#manage-passwords-button"
    );
    ok(
      ContentTaskUtils.is_visible(managePasswordsButton),
      "Manage passwords button is visible"
    );
    info(
      "Click on the manage passwords button and check that it opens about:logins in a new tab"
    );
    managePasswordsButton.click();
  });
  let loginsTab = await aboutLoginsPromise;
  info("about:logins was successfully opened in a new tab");
  gBrowser.removeTab(loginsTab);

  info(
    "Add another login and check that the scanned text about stored logins is updated after reload."
  );
  Services.logins.addLogin(TEST_LOGIN2);
  await reloadTab(tab);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    const lockwiseScannedText = content.document.querySelector(
      "#lockwise-scanned-text"
    ).textContent;
    ContentTaskUtils.waitForCondition(
      () =>
        lockwiseScannedText.textContent ==
        "Your passwords are being stored securely.",
      "Correct lockwise scanned text is shown"
    );
  });

  Services.logins.removeLogin(TEST_LOGIN1);
  Services.logins.removeLogin(TEST_LOGIN2);

  gBrowser.removeTab(tab);
});

add_task(async function testLockwiseCardUIWithBreachedLogins() {
  info(
    "Add a breached login and test that the lockwise scanned text is displayed correctly"
  );
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  Services.logins.addLogin(TEST_LOGIN1);

  info("Mock monitor data with a breached login to test the Lockwise UI");
  AboutProtectionsParent.setTestOverride(mockGetMonitorData(1));
  await reloadTab(tab);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    const lockwiseScannedText = content.document.querySelector(
      "#lockwise-scanned-text"
    );
    ok(
      ContentTaskUtils.is_visible(lockwiseScannedText),
      "Lockwise scanned text is visible"
    );
    await ContentTaskUtils.waitForCondition(
      () =>
        lockwiseScannedText.textContent ==
        "1 password may have been exposed in a data breach."
    );
    info("Correct lockwise scanned text is shown");
  });

  info(
    "Mock monitor data with more than one breached logins to test the Lockwise UI"
  );
  AboutProtectionsParent.setTestOverride(mockGetMonitorData(2));
  await reloadTab(tab);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    const lockwiseScannedText = content.document.querySelector(
      "#lockwise-scanned-text"
    );
    ok(
      ContentTaskUtils.is_visible(lockwiseScannedText),
      "Lockwise scanned text is visible"
    );
    await ContentTaskUtils.waitForCondition(
      () =>
        lockwiseScannedText.textContent ==
        "2 passwords may have been exposed in a data breach."
    );
    info("Correct lockwise scanned text is shown");
  });

  AboutProtectionsParent.setTestOverride(null);
  Services.logins.removeLogin(TEST_LOGIN1);
  gBrowser.removeTab(tab);
});

add_task(async function testLockwiseCardPref() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  info("Disable showing the Lockwise card.");
  Services.prefs.setBoolPref(
    "browser.contentblocking.report.lockwise.enabled",
    false
  );
  await reloadTab(tab);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    const lockwiseCard = content.document.querySelector(".lockwise-card");
    await ContentTaskUtils.waitForCondition(() => {
      return !lockwiseCard["data-enabled"];
    }, "Lockwise card is not enabled.");

    ok(ContentTaskUtils.is_hidden(lockwiseCard), "Lockwise card is hidden.");
  });

  // Set the pref back to displaying the card.
  Services.prefs.setBoolPref(
    "browser.contentblocking.report.lockwise.enabled",
    true
  );
  gBrowser.removeTab(tab);
});
