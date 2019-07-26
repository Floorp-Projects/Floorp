/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://testing-common/LoginTestUtils.jsm", this);

/**
 * Waits for the master password prompt and performs an action.
 * @param {string} action Set to "authenticate" to log in or "cancel" to
 *        close the dialog without logging in.
 */
function waitForMPDialog(action) {
  let dialogShown = TestUtils.topicObserved("common-dialog-loaded");
  return dialogShown.then(function([subject]) {
    let dialog = subject.Dialog;
    is(
      dialog.args.title,
      "Password Required",
      "Dialog is the Master Password dialog"
    );
    if (action == "authenticate") {
      SpecialPowers.wrap(dialog.ui.password1Textbox).setUserInput(
        LoginTestUtils.masterPassword.masterPassword
      );
      dialog.ui.button0.click();
    } else if (action == "cancel") {
      dialog.ui.button1.click();
    }
    return BrowserTestUtils.waitForEvent(window, "DOMModalDialogClosed");
  });
}

function waitForLoginCountToReach(browser, loginCount) {
  return ContentTask.spawn(browser, loginCount, async expectedLoginCount => {
    let loginList = Cu.waiveXrays(content.document.querySelector("login-list"));
    await ContentTaskUtils.waitForCondition(() => {
      return loginList._logins.length == expectedLoginCount;
    });
    return loginList._logins.length;
  });
}

add_task(async function test() {
  TEST_LOGIN1 = await addLogin(TEST_LOGIN1);
  LoginTestUtils.masterPassword.enable();

  let mpDialogShown = waitForMPDialog("cancel");
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  await mpDialogShown;

  registerCleanupFunction(function() {
    LoginTestUtils.masterPassword.disable();
    Services.logins.removeAllLogins();
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });

  let browser = gBrowser.selectedBrowser;
  let logins = await waitForLoginCountToReach(browser, 0);
  is(
    logins,
    0,
    "No logins should be displayed when MP is set and unauthenticated"
  );

  let notification;
  await BrowserTestUtils.waitForCondition(
    () =>
      (notification = gBrowser
        .getNotificationBox()
        .getNotificationWithValue("master-password-login-required"))
  );

  ok(
    notification,
    "master-password-login-required notification should be visible"
  );

  let buttons = notification.querySelectorAll(".notification-button");
  is(buttons.length, 1, "Should have one button.");

  let refreshPromise = BrowserTestUtils.browserLoaded(browser);
  // Sign in with the Master Password this time the dialog is shown
  mpDialogShown = waitForMPDialog("authenticate");
  // Click the button to reload the page.
  buttons[0].click();
  await refreshPromise;
  info("Page reloaded");

  await mpDialogShown;
  info("Master Password dialog shown and authenticated");

  logins = await waitForLoginCountToReach(browser, 1);
  is(logins, 1, "Logins should be displayed when MP is set and authenticated");
});
