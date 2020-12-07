/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

ChromeUtils.import("resource://testing-common/OSKeyStoreTestUtils.jsm", this);
const OS_REAUTH_PREF = "signon.management.page.os-auth.enabled";

async function openRemoveAllDialog(browser) {
  await SimpleTest.promiseFocus(browser);
  await BrowserTestUtils.synthesizeMouseAtCenter("menu-button", {}, browser);
  await SpecialPowers.spawn(browser, [], async () => {
    let menuButton = content.document.querySelector("menu-button");
    let menu = menuButton.shadowRoot.querySelector("ul.menu");
    await ContentTaskUtils.waitForCondition(() => !menu.hidden);
  });
  function getRemoveAllMenuButton() {
    let menuButton = window.document.querySelector("menu-button");
    return menuButton.shadowRoot.querySelector(".menuitem-remove-all-logins");
  }
  await BrowserTestUtils.synthesizeMouseAtCenter(
    getRemoveAllMenuButton,
    {},
    browser
  );
  info("remove all dialog should be opened");
}

async function activateLoginItemEdit(browser) {
  await SimpleTest.promiseFocus(browser);
  function getLoginItemEditButton() {
    let loginItem = window.document.querySelector("login-item");
    return loginItem.shadowRoot.querySelector(".edit-button");
  }
  await BrowserTestUtils.synthesizeMouseAtCenter(
    getLoginItemEditButton,
    {},
    browser
  );
  info("login-item should be in edit mode");
}

async function waitForRemoveAllLogins() {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(subject, topic, changeType) {
      if (changeType != "removeAllLogins") {
        return;
      }

      Services.obs.removeObserver(observer, "passwordmgr-storage-changed");
      resolve();
    }, "passwordmgr-storage-changed");
  });
}

add_task(async function setup() {
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllUserFacingLogins();
  });
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
});

add_task(async function test_remove_all_dialog_l10n() {
  ok(TEST_LOGIN1, "test_login1");
  let browser = gBrowser.selectedBrowser;
  await openRemoveAllDialog(browser);
  await SpecialPowers.spawn(browser, [], async () => {
    const EventUtils = ContentTaskUtils.getEventUtils(content);
    let dialog = Cu.waiveXrays(
      content.document.querySelector("remove-logins-dialog")
    );
    ok(!dialog.hidden);
    let title = dialog.shadowRoot.querySelector(".title");
    let message = dialog.shadowRoot.querySelector(".message");
    let label = dialog.shadowRoot.querySelector(
      "label[for='confirmation-checkbox']"
    );
    let cancelButton = dialog.shadowRoot.querySelector(".cancel-button");
    let removeAllButton = dialog.shadowRoot.querySelector(".confirm-button");
    await content.document.l10n.translateElements([
      title,
      message,
      label,
      cancelButton,
      removeAllButton,
    ]);
    is(
      title.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-title",
      "Title contents should match l10n-id attribute set on element"
    );
    is(
      message.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-message",
      "Message contents should match l10n-id attribute set on element"
    );
    is(
      label.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-checkbox-label",
      "Label contents should match l10n-id attribute set on outer element"
    );
    is(
      cancelButton.dataset.l10nId,
      "confirmation-dialog-cancel-button",
      "Cancel button contents should match l10n-id attribute set on outer element"
    );
    is(
      removeAllButton.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-confirm-button-label",
      "Remove all button contents should match l10n-id attribute set on outer element"
    );
    is(
      JSON.parse(title.dataset.l10nArgs).count,
      1,
      "Title contents should match l10n-args attribute set on element"
    );
    is(
      JSON.parse(message.dataset.l10nArgs).count,
      1,
      "Message contents should match l10n-args attribute set on element"
    );
    is(
      JSON.parse(label.dataset.l10nArgs).count,
      1,
      "Label contents should match l10n-id attribute set on outer element"
    );
    EventUtils.synthesizeMouseAtCenter(
      dialog.shadowRoot.querySelector(".cancel-button"),
      {},
      content
    );
    await ContentTaskUtils.waitForCondition(
      () => dialog.hidden,
      "Waiting for the dialog to be hidden after clicking cancel button"
    );
  });
});

add_task(async function test_remove_all_dialog_keyboard_navigation() {
  let browser = gBrowser.selectedBrowser;
  await openRemoveAllDialog(browser);
  await SpecialPowers.spawn(browser, [], async () => {
    const EventUtils = ContentTaskUtils.getEventUtils(content);
    let dialog = Cu.waiveXrays(
      content.document.querySelector("remove-logins-dialog")
    );
    let cancelButton = dialog.shadowRoot.querySelector(".cancel-button");
    let removeAllButton = dialog.shadowRoot.querySelector(".confirm-button");
    is(
      removeAllButton.disabled,
      true,
      "Remove all should be disabled on dialog open"
    );
    await EventUtils.synthesizeKey(" ", {}, content);
    is(
      removeAllButton.disabled,
      false,
      "Remove all should be enabled when activating the checkbox"
    );
    await EventUtils.synthesizeKey(" ", {}, content);
    is(
      removeAllButton.disabled,
      true,
      "Remove all should be disabled after deactivating the checkbox"
    );
    await EventUtils.synthesizeKey("KEY_Tab", {}, content);
    is(
      dialog.shadowRoot.activeElement,
      cancelButton,
      "Cancel button should be the next element in tab order"
    );
    await EventUtils.synthesizeKey(" ", {}, content);
    await ContentTaskUtils.waitForCondition(
      () => dialog.hidden,
      "Waiting for the dialog to be hidden after activating cancel button via Space key"
    );
  });
  await openRemoveAllDialog(browser);
  await SpecialPowers.spawn(browser, [], async () => {
    let dialog = Cu.waiveXrays(
      content.document.querySelector("remove-logins-dialog")
    );
    await EventUtils.synthesizeKey("KEY_Escape", {}, content);
    await ContentTaskUtils.waitForCondition(
      () => dialog.hidden,
      "Waiting for the dialog to be hidden after activating Escape key"
    );
  });
  await openRemoveAllDialog(browser);
  await SpecialPowers.spawn(browser, [], async () => {
    let dialog = Cu.waiveXrays(
      content.document.querySelector("remove-logins-dialog")
    );
    let dismissButton = dialog.shadowRoot.querySelector(".dismiss-button");
    await EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true }, content);
    is(
      dialog.shadowRoot.activeElement,
      dismissButton,
      "dismiss button should be focused"
    );
    await EventUtils.synthesizeKey(" ", {}, content);
    await ContentTaskUtils.waitForCondition(
      () => dialog.hidden,
      "Waiting for the dialog to be hidden after activating X button"
    );
  });
});

add_task(async function test_remove_all_dialog_remove_logins() {
  TEST_LOGIN2 = await addLogin(TEST_LOGIN2);
  let browser = gBrowser.selectedBrowser;
  let removeAllPromise = waitForRemoveAllLogins();

  await openRemoveAllDialog(browser);
  await SpecialPowers.spawn(browser, [], async () => {
    let dialog = Cu.waiveXrays(
      content.document.querySelector("remove-logins-dialog")
    );
    let title = dialog.shadowRoot.querySelector(".title");
    let message = dialog.shadowRoot.querySelector(".message");
    let label = dialog.shadowRoot.querySelector(
      "label[for='confirmation-checkbox']"
    );
    let cancelButton = dialog.shadowRoot.querySelector(".cancel-button");
    let removeAllButton = dialog.shadowRoot.querySelector(".confirm-button");

    let checkbox = dialog.shadowRoot.querySelector(".checkbox");

    await content.document.l10n.translateElements([
      title,
      message,
      cancelButton,
      removeAllButton,
      label,
      checkbox,
    ]);
    is(
      dialog.shadowRoot.activeElement,
      checkbox,
      "Checkbox should be the focused element on dialog open"
    );
    is(
      title.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-title",
      "Title contents should match l10n-id attribute set on element"
    );
    is(
      JSON.parse(title.dataset.l10nArgs).count,
      2,
      "Title contents should match l10n-args attribute set on element"
    );
    is(
      message.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-message",
      "Message contents should match l10n-id attribute set on element"
    );
    is(
      JSON.parse(message.dataset.l10nArgs).count,
      2,
      "Message contents should match l10n-args attribute set on element"
    );
    is(
      label.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-checkbox-label",
      "Label contents should match l10n-id attribute set on outer element"
    );
    is(
      JSON.parse(label.dataset.l10nArgs).count,
      2,
      "Label contents should match l10n-id attribute set on outer element"
    );
    is(
      cancelButton.dataset.l10nId,
      "confirmation-dialog-cancel-button",
      "Cancel button contents should match l10n-id attribute set on outer element"
    );
    is(
      removeAllButton.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-confirm-button-label",
      "Remove all button contents should match l10n-id attribute set on outer element"
    );
    is(
      removeAllButton.disabled,
      true,
      "Remove all button should be disabled on dialog open"
    );
  });
  function activateConfirmCheckbox() {
    let dialog = window.document.querySelector("remove-logins-dialog");
    return dialog.shadowRoot.querySelector(".checkbox");
  }

  await BrowserTestUtils.synthesizeMouseAtCenter(
    activateConfirmCheckbox,
    {},
    browser
  );

  await SpecialPowers.spawn(browser, [], async () => {
    let dialog = Cu.waiveXrays(
      content.document.querySelector("remove-logins-dialog")
    );
    let removeAllButton = dialog.shadowRoot.querySelector(".confirm-button");
    is(
      removeAllButton.disabled,
      false,
      "Remove all should be enabled after clicking the checkbox"
    );
  });
  function getDialogRemoveAllButton() {
    let dialog = window.document.querySelector("remove-logins-dialog");
    return dialog.shadowRoot.querySelector(".confirm-button");
  }
  await BrowserTestUtils.synthesizeMouseAtCenter(
    getDialogRemoveAllButton,
    {},
    browser
  );
  await removeAllPromise;
  await SpecialPowers.spawn(browser, [], async () => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    await ContentTaskUtils.waitForCondition(
      () => content.document.documentElement.classList.contains("no-logins"),
      "Waiting for no logins view since all logins should be deleted"
    );
    await ContentTaskUtils.waitForCondition(
      () =>
        !content.document.documentElement.classList.contains("login-selected"),
      "Waiting for the FxA Sync illustration to reappear"
    );
    await ContentTaskUtils.waitForCondition(
      () => loginList.classList.contains("no-logins"),
      "Waiting for login-list to be in no logins view as all logins should be deleted"
    );
  });
  await BrowserTestUtils.synthesizeMouseAtCenter("menu-button", {}, browser);
  await SpecialPowers.spawn(browser, [], async () => {
    let menuButton = content.document.querySelector("menu-button");
    let removeAllMenuButton = menuButton.shadowRoot.querySelector(
      ".menuitem-remove-all-logins"
    );
    ok(
      removeAllMenuButton.disabled,
      "Remove all logins menu button is disabled if there are no logins"
    );
  });
  await SpecialPowers.spawn(browser, [], async () => {
    let menuButton = Cu.waiveXrays(
      content.document.querySelector("menu-button")
    );
    let menu = menuButton.shadowRoot.querySelector("ul.menu");
    await EventUtils.synthesizeKey("KEY_Escape", {}, content);
    await ContentTaskUtils.waitForCondition(
      () => menu.hidden,
      "Waiting for menu to close"
    );
  });
});

add_task(async function test_ensure_edit_mode_reset_login_item() {
  // Preferences.set(OS_REAUTH_PREF, false);
  await SpecialPowers.pushPrefEnv({
    set: [[OS_REAUTH_PREF, false]],
  });
  TEST_LOGIN2 = await addLogin(TEST_LOGIN2);
  let browser = gBrowser.selectedBrowser;
  await activateLoginItemEdit(browser);
  await openRemoveAllDialog(browser);
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = content.document.querySelector("login-item");
    ok(
      !loginItem.dataset.editing,
      "Login item is no longer in edit mode due to remove all dialog being present"
    );
  });
  await SpecialPowers.popPrefEnv();
});
