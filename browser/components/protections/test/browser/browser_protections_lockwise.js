/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(2);

const { AboutProtectionsParent } = ChromeUtils.importESModule(
  "resource:///actors/AboutProtectionsParent.sys.mjs"
);
const ABOUT_LOGINS_URL = "about:logins";

add_task(async function testNoLoginsLockwiseCardUI() {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  const aboutLoginsPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    ABOUT_LOGINS_URL
  );

  info(
    "Check that the correct lockwise card content is displayed for non-logged in users."
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await ContentTaskUtils.waitForCondition(() => {
      const lockwiseCard = content.document.querySelector(".lockwise-card");
      return ContentTaskUtils.isVisible(lockwiseCard);
    }, "Lockwise card for user with no logins is visible.");

    const lockwiseHowItWorks = content.document.querySelector(
      "#lockwise-how-it-works"
    );
    ok(
      ContentTaskUtils.isHidden(lockwiseHowItWorks),
      "How it works link is hidden"
    );

    const lockwiseHeaderContent = content.document.querySelector(
      "#lockwise-header-content span"
    );
    await content.document.l10n.translateElements([lockwiseHeaderContent]);
    is(
      lockwiseHeaderContent.dataset.l10nId,
      "passwords-header-content",
      "lockwiseHeaderContent contents should match l10n-id attribute set on the element"
    );

    const lockwiseScannedWrapper = content.document.querySelector(
      ".lockwise-scanned-wrapper"
    );
    ok(
      ContentTaskUtils.isHidden(lockwiseScannedWrapper),
      "Lockwise scanned wrapper is hidden"
    );

    const managePasswordsButton = content.document.querySelector(
      "#manage-passwords-button"
    );
    ok(
      ContentTaskUtils.isHidden(managePasswordsButton),
      "Manage passwords button is hidden"
    );

    const savePasswordsButton = content.document.querySelector(
      "#save-passwords-button"
    );
    ok(
      ContentTaskUtils.isVisible(savePasswordsButton),
      "Save passwords button is visible in the header"
    );
    info(
      "Click on the save passwords button and check that it opens about:logins in a new tab"
    );
    savePasswordsButton.click();
  });
  const loginsTab = await aboutLoginsPromise;
  info("about:logins was successfully opened in a new tab");
  gBrowser.removeTab(loginsTab);
  gBrowser.removeTab(tab);
});

add_task(async function testLockwiseCardUIWithLogins() {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  const aboutLoginsPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    ABOUT_LOGINS_URL
  );

  info(
    "Add a login and check that lockwise card content for a logged in user is displayed correctly"
  );
  await Services.logins.addLoginAsync(TEST_LOGIN1);
  await BrowserTestUtils.reloadTab(tab);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await ContentTaskUtils.waitForCondition(() => {
      const hasLogins = content.document.querySelector(".lockwise-card");
      return ContentTaskUtils.isVisible(hasLogins);
    }, "Lockwise card for user with logins is visible");

    const lockwiseTitle = content.document.querySelector("#lockwise-title");
    await content.document.l10n.translateElements([lockwiseTitle]);
    await ContentTaskUtils.waitForCondition(
      () => lockwiseTitle.textContent == "Manage your passwords",
      "Waiting for Fluent to provide the title translation"
    );
    is(
      lockwiseTitle.textContent,
      "Manage your passwords",
      "Correct passwords title is shown"
    );

    const lockwiseHowItWorks = content.document.querySelector(
      "#lockwise-how-it-works"
    );
    ok(
      ContentTaskUtils.isVisible(lockwiseHowItWorks),
      "How it works link is visible"
    );

    const lockwiseHeaderContent = content.document.querySelector(
      "#lockwise-header-content span"
    );
    await content.document.l10n.translateElements([lockwiseHeaderContent]);
    is(
      lockwiseHeaderContent.dataset.l10nId,
      "lockwise-header-content-logged-in",
      "lockwiseHeaderContent contents should match l10n-id attribute set on the element"
    );

    const lockwiseScannedWrapper = content.document.querySelector(
      ".lockwise-scanned-wrapper"
    );
    ok(
      ContentTaskUtils.isVisible(lockwiseScannedWrapper),
      "Lockwise scanned wrapper is visible"
    );

    const lockwiseScannedText = content.document.querySelector(
      "#lockwise-scanned-text"
    );
    await content.document.l10n.translateElements([lockwiseScannedText]);
    is(
      lockwiseScannedText.textContent,
      "1 password stored securely.",
      "Correct lockwise scanned text is shown"
    );

    const savePasswordsButton = content.document.querySelector(
      "#save-passwords-button"
    );
    ok(
      ContentTaskUtils.isHidden(savePasswordsButton),
      "Save passwords button is hidden"
    );

    const managePasswordsButton = content.document.querySelector(
      "#manage-passwords-button"
    );
    ok(
      ContentTaskUtils.isVisible(managePasswordsButton),
      "Manage passwords button is visible"
    );
    info(
      "Click on the manage passwords button and check that it opens about:logins in a new tab"
    );
    managePasswordsButton.click();
  });
  const loginsTab = await aboutLoginsPromise;
  info("about:logins was successfully opened in a new tab");
  gBrowser.removeTab(loginsTab);

  info(
    "Add another login and check that the scanned text about stored logins is updated after reload."
  );
  await Services.logins.addLoginAsync(TEST_LOGIN2);
  await BrowserTestUtils.reloadTab(tab);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
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
  const tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });
  await Services.logins.addLoginAsync(TEST_LOGIN1);

  info("Mock monitor data with a breached login to test the Lockwise UI");
  AboutProtectionsParent.setTestOverride(
    mockGetLoginDataWithSyncedDevices(false, 1)
  );
  await BrowserTestUtils.reloadTab(tab);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    const lockwiseScannedText = content.document.querySelector(
      "#lockwise-scanned-text"
    );
    ok(
      ContentTaskUtils.isVisible(lockwiseScannedText),
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
  AboutProtectionsParent.setTestOverride(
    mockGetLoginDataWithSyncedDevices(false, 2)
  );
  await BrowserTestUtils.reloadTab(tab);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    const lockwiseScannedText = content.document.querySelector(
      "#lockwise-scanned-text"
    );
    ok(
      ContentTaskUtils.isVisible(lockwiseScannedText),
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
  const tab = await BrowserTestUtils.openNewForegroundTab({
    url: "about:protections",
    gBrowser,
  });

  info("Disable showing the Lockwise card.");
  Services.prefs.setBoolPref(
    "browser.contentblocking.report.lockwise.enabled",
    false
  );
  await BrowserTestUtils.reloadTab(tab);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    const lockwiseCard = content.document.querySelector(".lockwise-card");
    await ContentTaskUtils.waitForCondition(() => {
      return !lockwiseCard["data-enabled"];
    }, "Lockwise card is not enabled.");

    ok(ContentTaskUtils.isHidden(lockwiseCard), "Lockwise card is hidden.");
  });

  // Set the pref back to displaying the card.
  Services.prefs.setBoolPref(
    "browser.contentblocking.report.lockwise.enabled",
    true
  );
  gBrowser.removeTab(tab);
});
