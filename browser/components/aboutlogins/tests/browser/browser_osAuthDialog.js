/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test() {
  info(
    `updatechannel: ${UpdateUtils.getUpdateChannel(false)}; platform: ${
      AppConstants.platform
    }`
  );
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    Assert.ok(
      true,
      `skipping test since oskeystore cannot be automated in this environment`
    );
    return;
  }

  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);

  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });

  registerCleanupFunction(function () {
    Services.logins.removeAllUserFacingLogins();
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });

  // Show OS auth dialog when Reveal Password checkbox is checked if not on a new login
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
  osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
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

  info("'Edit' shouldn't show the prompt since the user has authenticated now");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
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

  info("Test that the OS auth prompt is shown after about:logins is reopened");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });

  // Show OS auth dialog since the page has been reloaded.
  osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(false);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    revealCheckbox.click();
  });
  await osAuthDialogShown;
  info("OS auth dialog shown and canceled");

  // Show OS auth dialog since the previous attempt was canceled
  osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    revealCheckbox.click();
    info("clicking on reveal checkbox to hide the password");
    revealCheckbox.click();
  });
  await osAuthDialogShown;
  info("OS auth dialog shown and passed");

  // Show OS auth dialog since the timeout will have expired
  osAuthDialogShown = forceAuthTimeoutAndWaitForOSKeyStoreLogin({
    loginResult: true,
  });
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    info("clicking on reveal checkbox to reveal password");
    revealCheckbox.click();
  });
  info("waiting for os auth dialog");
  await osAuthDialogShown;
  info("OS auth dialog shown and passed after timeout expiration");

  // Disable the OS auth feature and confirm the prompt doesn't appear
  await SpecialPowers.pushPrefEnv({
    set: [["signon.management.page.os-auth.enabled", false]],
  });
  info("Reload about:logins to reset the timeout");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });

  info("'Edit' shouldn't show the prompt since the feature has been disabled");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
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
});
