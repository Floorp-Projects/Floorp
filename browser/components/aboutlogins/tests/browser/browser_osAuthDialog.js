/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://testing-common/OSKeyStoreTestUtils.jsm", this);

add_task(async function test() {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);

  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });

  registerCleanupFunction(function() {
    Services.logins.removeAllLogins();
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });

  // Show OS auth dialog when Reveal Password checkbox is checked if not on a new login
  let osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(false);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    revealCheckbox.click();
  });
  await osAuthDialogShown;
  info("OS auth dialog shown and canceled");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    ok(
      !revealCheckbox.checked,
      "reveal checkbox should be unchecked if OS auth dialog canceled"
    );
  });
  osAuthDialogShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    revealCheckbox.click();
  });
  await osAuthDialogShown;
  info("OS auth dialog shown and authenticated");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let loginItem = content.document.querySelector("login-item");
    let revealCheckbox = loginItem.shadowRoot.querySelector(
      ".reveal-password-checkbox"
    );
    ok(
      revealCheckbox.checked,
      "reveal checkbox should be checked if OS auth dialog authenticated"
    );
  });
});
