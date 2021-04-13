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

add_task(async function not_now() {
  await SpecialPowers.pushPrefEnv({
    set: [[CHECK_PREF, true]],
  });

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
  await SpecialPowers.pushPrefEnv({
    set: [[CHECK_PREF, true]],
  });

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
