/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function () {
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

add_task(async function test() {
  let browser = gBrowser.selectedBrowser;

  await SpecialPowers.spawn(browser, [], async () => {
    let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));

    let showPromise = loginItem.showConfirmationDialog("delete");

    let dialog = Cu.waiveXrays(
      content.document.querySelector("confirmation-dialog")
    );
    let cancelButton = dialog.shadowRoot.querySelector(".cancel-button");
    let confirmDeleteButton =
      dialog.shadowRoot.querySelector(".confirm-button");
    let dismissButton = dialog.shadowRoot.querySelector(".dismiss-button");
    let message = dialog.shadowRoot.querySelector(".message");
    let title = dialog.shadowRoot.querySelector(".title");

    await content.document.l10n.translateElements([
      title,
      message,
      cancelButton,
      confirmDeleteButton,
    ]);

    Assert.equal(
      title.dataset.l10nId,
      "about-logins-confirm-delete-dialog-title",
      "Title contents should match l10n attribute set on outer element"
    );
    Assert.equal(
      message.dataset.l10nId,
      "about-logins-confirm-delete-dialog-message",
      "Message contents should match l10n attribute set on outer element"
    );
    Assert.equal(
      cancelButton.dataset.l10nId,
      "confirmation-dialog-cancel-button",
      "Cancel button contents should match l10n attribute set on outer element"
    );
    Assert.equal(
      confirmDeleteButton.dataset.l10nId,
      "about-logins-confirm-remove-dialog-confirm-button",
      "Remove button contents should match l10n attribute set on outer element"
    );

    cancelButton.click();
    try {
      await showPromise;
      Assert.ok(
        false,
        "Promise returned by show() should not resolve after clicking cancel button"
      );
    } catch (ex) {
      Assert.ok(
        true,
        "Promise returned by show() should reject after clicking cancel button"
      );
    }
    await ContentTaskUtils.waitForCondition(
      () => dialog.hidden,
      "Waiting for the dialog to be hidden"
    );
    Assert.ok(
      dialog.hidden,
      "Dialog should be hidden after clicking cancel button"
    );

    showPromise = loginItem.showConfirmationDialog("delete");
    dismissButton.click();
    try {
      await showPromise;
      Assert.ok(
        false,
        "Promise returned by show() should not resolve after clicking dismiss button"
      );
    } catch (ex) {
      Assert.ok(
        true,
        "Promise returned by show() should reject after clicking dismiss button"
      );
    }
    await ContentTaskUtils.waitForCondition(
      () => dialog.hidden,
      "Waiting for the dialog to be hidden"
    );
    Assert.ok(
      dialog.hidden,
      "Dialog should be hidden after clicking dismiss button"
    );

    showPromise = loginItem.showConfirmationDialog("delete");
    confirmDeleteButton.click();
    try {
      await showPromise;
      Assert.ok(
        true,
        "Promise returned by show() should resolve after clicking confirm button"
      );
    } catch (ex) {
      Assert.ok(
        false,
        "Promise returned by show() should not reject after clicking confirm button"
      );
    }
    await ContentTaskUtils.waitForCondition(
      () => dialog.hidden,
      "Waiting for the dialog to be hidden"
    );
    Assert.ok(
      dialog.hidden,
      "Dialog should be hidden after clicking confirm button"
    );
  });
});
