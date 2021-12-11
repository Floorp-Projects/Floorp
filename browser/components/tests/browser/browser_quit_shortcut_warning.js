/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks that the quit dialog appears correctly when the browser.warnOnQuitShortcut
// preference is set and the quit keyboard shortcut is pressed.
add_task(async function test_quit_shortcut() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.warnOnQuit", true],
      ["browser.warnOnQuitShortcut", true],
    ],
  });

  function checkDialog(dialog) {
    let dialogElement = dialog.document.getElementById("commonDialog");
    let acceptLabel = dialogElement.getButton("accept").label;
    is(acceptLabel.indexOf("Quit"), 0, "dialog label");
    dialogElement.getButton("cancel").click();
  }

  let dialogOpened = false;
  function setDialogOpened() {
    dialogOpened = true;
  }
  Services.obs.addObserver(setDialogOpened, "common-dialog-loaded");

  // Test 1: quit using the shortcut key with the preference enabled.
  let quitPromise = BrowserTestUtils.promiseAlertDialog("cancel", undefined, {
    callback: checkDialog,
  });
  ok(!canQuitApplication(undefined, "shortcut"), "can quit with dialog");

  ok(dialogOpened, "confirmation prompt should have opened");

  await quitPromise;

  // Test 2: quit without using the shortcut key with the preference enabled.
  dialogOpened = false;
  ok(canQuitApplication(undefined, ""), "can quit with no dialog");
  ok(!dialogOpened, "confirmation prompt should not have opened");

  // Test 3: quit using the shortcut key with the preference disabled.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.warnOnQuitShortcut", false]],
  });

  dialogOpened = false;
  ok(canQuitApplication(undefined, "shortcut"), "can quit with no dialog");

  ok(!dialogOpened, "confirmation prompt should not have opened");
  Services.obs.removeObserver(setDialogOpened, "common-dialog-loaded");

  await SpecialPowers.popPrefEnv();
});
