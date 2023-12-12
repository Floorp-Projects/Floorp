/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

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
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = content.document.querySelector("login-item");
    Assert.ok(loginItem, "Login item should exist");
  });
  function getLoginItemEditButton() {
    let loginItem = window.document.querySelector("login-item");
    return loginItem.shadowRoot.querySelector("edit-button");
  }
  await BrowserTestUtils.synthesizeMouseAtCenter(
    getLoginItemEditButton,
    {},
    browser
  );
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = content.document.querySelector("login-item");
    let editButton = loginItem.shadowRoot
      .querySelector("edit-button")
      .shadowRoot.querySelector("button");
    editButton.click();
    await ContentTaskUtils.waitForCondition(
      () => loginItem.dataset.editing,
      "Waiting for login-item to enter edit mode"
    );
  });
  info("login-item should be in edit mode");
}

async function activateCreateNewLogin(browser) {
  await SimpleTest.promiseFocus(browser);
  function getCreateNewLoginButton() {
    let loginList = window.document.querySelector("login-list");
    return loginList.shadowRoot.querySelector("create-login-button");
  }
  await BrowserTestUtils.synthesizeMouseAtCenter(
    getCreateNewLoginButton,
    {},
    browser
  );
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

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [[OS_REAUTH_PREF, false]],
  });
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    Services.logins.removeAllUserFacingLogins();
    await SpecialPowers.popPrefEnv();
  });
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
});

add_task(async function test_remove_all_dialog_l10n() {
  Assert.ok(TEST_LOGIN1, "test_login1");
  let browser = gBrowser.selectedBrowser;
  await openRemoveAllDialog(browser);
  await SpecialPowers.spawn(browser, [], async () => {
    const EventUtils = ContentTaskUtils.getEventUtils(content);
    let dialog = Cu.waiveXrays(
      content.document.querySelector("remove-logins-dialog")
    );
    Assert.ok(!dialog.hidden);
    let title = dialog.shadowRoot.querySelector(".title");
    let message = dialog.shadowRoot.querySelector(".message");
    let label = dialog.shadowRoot.querySelector(".checkbox-text");
    let cancelButton = dialog.shadowRoot.querySelector(".cancel-button");
    let removeAllButton = dialog.shadowRoot.querySelector(".confirm-button");
    await content.document.l10n.translateElements([
      title,
      message,
      label,
      cancelButton,
      removeAllButton,
    ]);
    Assert.equal(
      title.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-title2",
      "Title contents should match l10n-id attribute set on element"
    );
    Assert.equal(
      message.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-message2",
      "Message contents should match l10n-id attribute set on element"
    );
    Assert.equal(
      label.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-checkbox-label2",
      "Label contents should match l10n-id attribute set on outer element"
    );
    Assert.equal(
      cancelButton.dataset.l10nId,
      "confirmation-dialog-cancel-button",
      "Cancel button contents should match l10n-id attribute set on outer element"
    );
    Assert.equal(
      removeAllButton.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-confirm-button-label",
      "Remove all button contents should match l10n-id attribute set on outer element"
    );
    Assert.equal(
      JSON.parse(title.dataset.l10nArgs).count,
      1,
      "Title contents should match l10n-args attribute set on element"
    );
    Assert.equal(
      JSON.parse(message.dataset.l10nArgs).count,
      1,
      "Message contents should match l10n-args attribute set on element"
    );
    Assert.equal(
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
    Assert.equal(
      removeAllButton.disabled,
      true,
      "Remove all should be disabled on dialog open"
    );
    await EventUtils.synthesizeKey(" ", {}, content);
    Assert.equal(
      removeAllButton.disabled,
      false,
      "Remove all should be enabled when activating the checkbox"
    );
    await EventUtils.synthesizeKey(" ", {}, content);
    Assert.equal(
      removeAllButton.disabled,
      true,
      "Remove all should be disabled after deactivating the checkbox"
    );
    await EventUtils.synthesizeKey("KEY_Tab", {}, content);
    Assert.equal(
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
    Assert.equal(
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
    let label = dialog.shadowRoot.querySelector(".checkbox-text");
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
    Assert.equal(
      dialog.shadowRoot.activeElement,
      checkbox,
      "Checkbox should be the focused element on dialog open"
    );
    Assert.equal(
      title.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-title2",
      "Title contents should match l10n-id attribute set on element"
    );
    Assert.equal(
      JSON.parse(title.dataset.l10nArgs).count,
      2,
      "Title contents should match l10n-args attribute set on element"
    );
    Assert.equal(
      message.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-message2",
      "Message contents should match l10n-id attribute set on element"
    );
    Assert.equal(
      JSON.parse(message.dataset.l10nArgs).count,
      2,
      "Message contents should match l10n-args attribute set on element"
    );
    Assert.equal(
      label.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-checkbox-label2",
      "Label contents should match l10n-id attribute set on outer element"
    );
    Assert.equal(
      JSON.parse(label.dataset.l10nArgs).count,
      2,
      "Label contents should match l10n-id attribute set on outer element"
    );
    Assert.equal(
      cancelButton.dataset.l10nId,
      "confirmation-dialog-cancel-button",
      "Cancel button contents should match l10n-id attribute set on outer element"
    );
    Assert.equal(
      removeAllButton.dataset.l10nId,
      "about-logins-confirm-remove-all-dialog-confirm-button-label",
      "Remove all button contents should match l10n-id attribute set on outer element"
    );
    Assert.equal(
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
    Assert.equal(
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
    Assert.ok(
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

add_task(async function test_edit_mode_resets_on_remove_all_with_login() {
  TEST_LOGIN2 = await addLogin(TEST_LOGIN2);
  let removeAllPromise = waitForRemoveAllLogins();
  let browser = gBrowser.selectedBrowser;
  await activateLoginItemEdit(browser);
  await openRemoveAllDialog(browser);
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = content.document.querySelector("login-item");
    Assert.ok(
      loginItem.dataset.editing,
      "Login item is still in edit mode when the remove all dialog opens"
    );
  });
  function getDialogCancelButton() {
    let dialog = window.document.querySelector("remove-logins-dialog");
    return dialog.shadowRoot.querySelector(".cancel-button");
  }
  await BrowserTestUtils.synthesizeMouseAtCenter(
    getDialogCancelButton,
    {},
    browser
  );
  await TestUtils.waitForTick();
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = content.document.querySelector("login-item");
    Assert.ok(
      loginItem.dataset.editing,
      "Login item should be in editing mode after activating the cancel button in the remove all dialog"
    );
  });

  await openRemoveAllDialog(browser);
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
    Assert.equal(
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
  await TestUtils.waitForTick();
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = content.document.querySelector("login-item");
    Assert.ok(
      !loginItem.dataset.editing,
      "Login item should not be in editing mode after activating the confirm button in the remove all dialog"
    );
  });
  await removeAllPromise;
});

add_task(async function test_remove_all_when_creating_new_login() {
  TEST_LOGIN2 = await addLogin(TEST_LOGIN2);
  let removeAllPromise = waitForRemoveAllLogins();
  let browser = gBrowser.selectedBrowser;
  await activateCreateNewLogin(browser);
  await openRemoveAllDialog(browser);
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = content.document.querySelector("login-item");
    Assert.ok(
      loginItem.dataset.editing,
      "Login item should be in edit mode when the remove all dialog opens"
    );
    Assert.ok(
      loginItem.dataset.isNewLogin,
      "Login item should be in the 'new login' state when the remove all dialog opens"
    );
  });
  function getDialogCancelButton() {
    let dialog = window.document.querySelector("remove-logins-dialog");
    return dialog.shadowRoot.querySelector(".cancel-button");
  }
  await BrowserTestUtils.synthesizeMouseAtCenter(
    getDialogCancelButton,
    {},
    browser
  );
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = content.document.querySelector("login-item");
    Assert.ok(
      loginItem.dataset.editing,
      "Login item is still in edit mode after cancelling out of the remove all dialog"
    );
    Assert.ok(
      loginItem.dataset.isNewLogin,
      "Login item should be in the 'newLogin' state after cancelling out of the remove all dialog"
    );
  });

  await openRemoveAllDialog(browser);
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
    Assert.equal(
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
  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = content.document.querySelector("login-item");
    Assert.ok(
      !loginItem.dataset.editing,
      "Login item should not be in editing mode after activating the confirm button in the remove all dialog"
    );
    Assert.ok(
      !loginItem.dataset.isNewLogin,
      "Login item should not be in 'new login' mode after activating the confirm button in the remove all dialog"
    );
  });
  await removeAllPromise;
});

add_task(async function test_ensure_icons_are_not_draggable() {
  TEST_LOGIN2 = await addLogin(TEST_LOGIN2);
  let browser = gBrowser.selectedBrowser;
  await openRemoveAllDialog(browser);
  await SpecialPowers.spawn(browser, [], async () => {
    let dialog = content.document.querySelector("remove-logins-dialog");
    let warningIcon = dialog.shadowRoot.querySelector(".warning-icon");
    Assert.ok(!warningIcon.draggable, "Warning icon should not be draggable");
    let dismissIcon = dialog.shadowRoot.querySelector(".dismiss-icon");
    Assert.ok(!dismissIcon.draggable, "Dismiss icon should not be draggable");
  });
});
