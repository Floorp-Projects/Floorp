/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DefaultBrowserCheck } = ChromeUtils.import(
  "resource:///modules/BrowserGlue.jsm"
);
const CHECK_PREF = "browser.shell.checkDefaultBrowser";

function showAndWaitForModal(callback) {
  const promise = BrowserTestUtils.promiseAlertDialog(null, undefined, {
    callback,
    isSubDialog: true,
  });
  DefaultBrowserCheck.prompt(BrowserWindowTracker.getTopWindow());
  return promise;
}

add_task(async function proton_shows_prompt() {
  ShellService._checkedThisSession = false;
  await SpecialPowers.pushPrefEnv({
    set: [
      [CHECK_PREF, true],
      ["browser.defaultbrowser.notificationbar", true],
      ["browser.proton.enabled", true],
      ["browser.shell.didSkipDefaultBrowserCheckOnFirstRun", true],
    ],
  });

  const willPrompt = await DefaultBrowserCheck.willCheckDefaultBrowser();

  Assert.equal(
    willPrompt,
    !AppConstants.DEBUG,
    "Show default browser prompt with proton on non-debug builds"
  );
});

add_task(async function not_proton_shows_notificationbar() {
  Services.prefs.setBoolPref("browser.proton.enabled", false);

  const willPrompt = await DefaultBrowserCheck.willCheckDefaultBrowser();

  Assert.ok(
    !willPrompt,
    "Not proton shows notificationbar as set in previous task"
  );
});

add_task(async function not_now() {
  await showAndWaitForModal(win => {
    win.document
      .querySelector("dialog")
      .getButton("cancel")
      .click();
  });

  Assert.equal(
    Services.prefs.getBoolPref(CHECK_PREF),
    true,
    "Canceling keeps pref true"
  );
});

add_task(async function stop_asking() {
  await showAndWaitForModal(win => {
    const dialog = win.document.querySelector("dialog");
    dialog.querySelector("checkbox").click();
    dialog.getButton("cancel").click();
  });

  Assert.equal(
    Services.prefs.getBoolPref(CHECK_PREF),
    false,
    "Canceling with checkbox checked clears the pref"
  );
});
