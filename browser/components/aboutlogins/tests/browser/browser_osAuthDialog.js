/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// On mac, this test times out in chaos mode
requestLongerTimeout(2);

const PAGE_PREFS = "about:preferences";
const PAGE_PRIVACY = PAGE_PREFS + "#privacy";
const SELECTORS = {
  reauthCheckbox: "#osReauthCheckbox",
};

add_setup(async function () {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  TEST_LOGIN2 = await addLogin(TEST_LOGIN2);
  // Undo mocking from head.js
  sinon.restore();
});

add_task(async function test_os_auth_enabled_with_checkbox() {
  let finalPrefPaneLoaded = TestUtils.topicObserved("sync-pane-loaded");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function (browser) {
      await finalPrefPaneLoaded;

      await SpecialPowers.spawn(
        browser,
        [SELECTORS, AppConstants.NIGHTLY_BUILD],
        async (selectors, isNightly) => {
          is(
            content.document.querySelector(selectors.reauthCheckbox).checked,
            isNightly,
            "OSReauth for Passwords should be checked"
          );
        }
      );
      is(
        LoginHelper.getOSAuthEnabled(PASSWORDS_OS_REAUTH_PREF),
        AppConstants.NIGHTLY_BUILD,
        "OSAuth should be enabled."
      );
    }
  );
});

add_task(async function test_os_auth_disabled_with_checkbox() {
  let finalPrefPaneLoaded = TestUtils.topicObserved("sync-pane-loaded");
  LoginHelper.setOSAuthEnabled(PASSWORDS_OS_REAUTH_PREF, false);
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function (browser) {
      await finalPrefPaneLoaded;

      await SpecialPowers.spawn(browser, [SELECTORS], async selectors => {
        is(
          content.document.querySelector(selectors.reauthCheckbox).checked,
          false,
          "OSReauth for passwords should be unchecked"
        );
      });
      is(
        LoginHelper.getOSAuthEnabled(PASSWORDS_OS_REAUTH_PREF),
        false,
        "OSAuth should be disabled"
      );
    }
  );
  LoginHelper.setOSAuthEnabled(PASSWORDS_OS_REAUTH_PREF, true);
});

add_task(async function test_OSAuth_enabled_with_random_value_in_pref() {
  let finalPrefPaneLoaded = TestUtils.topicObserved("sync-pane-loaded");
  await SpecialPowers.pushPrefEnv({
    set: [[PASSWORDS_OS_REAUTH_PREF, "poutine-gravy"]],
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function (browser) {
      await finalPrefPaneLoaded;
      await SpecialPowers.spawn(browser, [SELECTORS], async selectors => {
        let reauthCheckbox = content.document.querySelector(
          selectors.reauthCheckbox
        );
        is(
          reauthCheckbox.checked,
          true,
          "OSReauth for passwords should be checked"
        );
      });
      is(
        LoginHelper.getOSAuthEnabled(PASSWORDS_OS_REAUTH_PREF),
        true,
        "OSAuth should be enabled since the pref does not decrypt to 'opt out'."
      );
    }
  );
});

add_task(async function test_osAuth_shown_on_edit_login() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    Assert.ok(
      true,
      `skipping test since oskeystore cannot be automated in this environment`
    );
    return;
  }
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  let osAuthDialogShown = Promise.resolve();
  if (OSKeyStore.canReauth()) {
    osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  }
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    Assert.ok(
      !loginItem.dataset.editing,
      "Not in edit mode before clicking 'Edit'"
    );
    let editButton = loginItem.shadowRoot.querySelector("edit-button");
    editButton.click();
  });

  await osAuthDialogShown;
  info("OS auth dialog shown and authenticated");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector("login-item").dataset.editing,
      "login item should be in 'edit' mode"
    );
  });
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_osAuth_shown_on_reveal_password() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    Assert.ok(
      true,
      `skipping test since oskeystore cannot be automated in this environment`
    );
    return;
  }
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  let osAuthDialogShown = Promise.resolve();
  if (OSKeyStore.canReauth()) {
    osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  }
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    revealCheckbox.click();
  });
  await osAuthDialogShown;
  info("OS auth dialog shown and authenticated");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    Assert.ok(
      revealCheckbox.checked,
      "reveal checkbox should be checked if OS auth dialog authenticated"
    );
  });
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_osAuth_shown_on_copy_password() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    Assert.ok(
      true,
      `skipping test since oskeystore cannot be automated in this environment`
    );
    return;
  }
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  let osAuthDialogShown = Promise.resolve();
  if (OSKeyStore.canReauth()) {
    osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  }
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let copyPassword = loginItem.shadowRoot.querySelector(
      "copy-password-button"
    );
    copyPassword.click();
  });
  await osAuthDialogShown;
  info("OS auth dialog shown and authenticated");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    info("Password was copied to clipboard");
  });
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_osAuth_not_shown_within_expiration_time() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    Assert.ok(
      true,
      `skipping test since oskeystore cannot be automated in this environment`
    );
    return;
  }
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  let osAuthDialogShown = Promise.resolve();
  if (OSKeyStore.canReauth()) {
    osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  }
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let copyPassword = loginItem.shadowRoot.querySelector(
      "copy-password-button"
    );
    copyPassword.click();
  });
  await osAuthDialogShown;
  info("OS auth dialog shown and authenticated");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    info(
      "'Edit' shouldn't show the prompt since the user has authenticated now"
    );
    let loginItem = content.document.querySelector("login-item");
    Assert.ok(
      !loginItem.dataset.editing,
      "Not in edit mode before clicking 'Edit'"
    );
    let editButton = loginItem.shadowRoot.querySelector("edit-button");
    editButton.click();

    await ContentTaskUtils.waitForCondition(
      () => loginItem.dataset.editing,
      "waiting for 'edit' mode"
    );
    Assert.ok(loginItem.dataset.editing, "In edit mode");
  });
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_osAuth_shown_after_expiration_timeout() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    Assert.ok(
      true,
      `skipping test since oskeystore cannot be automated in this environment`
    );
    return;
  }
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  let osAuthDialogShown = Promise.resolve();
  if (OSKeyStore.canReauth()) {
    osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  }
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let copyPassword = loginItem.shadowRoot.querySelector(
      "copy-password-button"
    );
    copyPassword.click();
  });
  await osAuthDialogShown;
  info("OS auth dialog shown and authenticated");

  // Show OS auth dialog since the timeout will have expired

  if (OSKeyStore.canReauth()) {
    osAuthDialogShown = forceAuthTimeoutAndWaitForOSKeyStoreLogin({
      loginResult: true,
    });
  }

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    revealCheckbox.click();
  });
  await osAuthDialogShown;
  info("OS auth dialog shown and authenticated");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_osAuth_shown_on_reload() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    Assert.ok(
      true,
      `skipping test since oskeystore cannot be automated in this environment`
    );
    return;
  }
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  let osAuthDialogShown = Promise.resolve();
  if (OSKeyStore.canReauth()) {
    osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  }
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let copyPassword = loginItem.shadowRoot.querySelector(
      "copy-password-button"
    );
    copyPassword.click();
  });
  await osAuthDialogShown;
  info("OS auth dialog shown and authenticated");

  info("Test that the OS auth prompt is shown after about:logins is reopened");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });

  // Show OS auth dialog since the page has been reloaded.
  osAuthDialogShown = Promise.resolve();
  if (OSKeyStore.canReauth()) {
    osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  }
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    revealCheckbox.click();
  });
  await osAuthDialogShown;
  info("OS auth dialog shown and authenticated");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_osAuth_shown_again_on_cancel() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    Assert.ok(
      true,
      `skipping test since oskeystore cannot be automated in this environment`
    );
    return;
  }
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  let osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(false);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    revealCheckbox.click();
  });
  await osAuthDialogShown;
  info("OS auth dialog shown and canceled");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    Assert.ok(
      !revealCheckbox.checked,
      "reveal checkbox should be unchecked if OS auth dialog canceled"
    );
  });
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
