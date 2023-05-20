/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DefaultBrowserCheck } = ChromeUtils.importESModule(
  "resource:///modules/BrowserGlue.sys.mjs"
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

const TELEMETRY_NAMES = ["accept check", "accept", "cancel check", "cancel"];
function AssertHistogram(histogram, name, expect = 1) {
  TelemetryTestUtils.assertHistogram(
    histogram,
    TELEMETRY_NAMES.indexOf(name),
    expect
  );
}
function getHistogram() {
  return TelemetryTestUtils.getAndClearHistogram("BROWSER_SET_DEFAULT_RESULT");
}

add_task(async function proton_shows_prompt() {
  mockShell();
  ShellService._checkedThisSession = false;

  await SpecialPowers.pushPrefEnv({
    set: [
      [CHECK_PREF, true],
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

add_task(async function not_now() {
  const histogram = getHistogram();
  await showAndWaitForModal(win => {
    win.document.querySelector("dialog").getButton("cancel").click();
  });

  Assert.equal(
    Services.prefs.getBoolPref(CHECK_PREF),
    true,
    "Canceling keeps pref true"
  );
  AssertHistogram(histogram, "cancel");
});

add_task(async function stop_asking() {
  const histogram = getHistogram();

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
  AssertHistogram(histogram, "cancel check");
});

add_task(async function primary_default() {
  const mock = mockShell({ isPinned: true });
  const histogram = getHistogram();

  await showAndWaitForModal(win => {
    win.document.querySelector("dialog").getButton("accept").click();
  });

  Assert.equal(
    mock.setAsDefault.callCount,
    1,
    "Primary button sets as default"
  );
  Assert.equal(
    mock.pinCurrentAppToTaskbarAsync.callCount,
    0,
    "Primary button doesn't pin if already pinned"
  );
  AssertHistogram(histogram, "accept");
});

add_task(async function primary_pin() {
  const mock = mockShell({ canPin: true });
  const histogram = getHistogram();

  await showAndWaitForModal(win => {
    win.document.querySelector("dialog").getButton("accept").click();
  });

  Assert.equal(
    mock.setAsDefault.callCount,
    1,
    "Primary button sets as default"
  );
  if (AppConstants.platform == "win") {
    Assert.equal(
      mock.pinCurrentAppToTaskbarAsync.callCount,
      1,
      "Primary button also pins"
    );
  }
  AssertHistogram(histogram, "accept");
});
