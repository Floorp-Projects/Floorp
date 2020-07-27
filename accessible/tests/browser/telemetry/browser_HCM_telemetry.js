/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../../browser/components/preferences/tests/head.js */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/preferences/tests/head.js",
  this
);

ChromeUtils.import("resource://testing-common/TelemetryTestUtils.jsm", this);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.display.document_color_use");
  Services.prefs.clearUserPref("browser.display.permit_backplate");
  Services.telemetry.clearEvents();
});

async function openColorsDialog() {
  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  const colorsButton = gBrowser.selectedBrowser.contentDocument.getElementById(
    "colors"
  );

  const dialogOpened = promiseLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/colors.xhtml"
  );
  colorsButton.doCommand();

  return dialogOpened;
}

async function closeColorsDialog(dialogWin) {
  const dialogClosed = BrowserTestUtils.waitForEvent(dialogWin, "unload");
  dialogWin.document
    .getElementById("ColorsDialog")
    .getButton("accept")
    .doCommand();
  return dialogClosed;
}

function verifyBackplate(expectedValue) {
  const snapshot = TelemetryTestUtils.getProcessScalars("parent", false, true);
  ok("a11y.backplate" in snapshot, "Backplate scalar must be present.");
  is(
    snapshot["a11y.backplate"],
    expectedValue,
    "Backplate scalar is logged as " + expectedValue
  );
}

add_task(async function testInit() {
  const dialogWin = await openColorsDialog();
  const menulistHCM = dialogWin.document.getElementById("useDocumentColors");
  if (AppConstants.platform == "win") {
    is(
      menulistHCM.value,
      "0",
      "HCM menulist should be set to only with HCM theme on startup for windows"
    );

    // Verify correct default value on windows
    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true, true),
      "a11y.theme",
      "default",
      false
    );
  } else {
    is(
      menulistHCM.value,
      "1",
      "HCM menulist should be set to never on startup for non-windows platforms"
    );

    // Verify correct default value on non-windows
    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true, true),
      "a11y.theme",
      "always",
      false
    );
  }

  gBrowser.removeCurrentTab();
});

add_task(async function testSetAlways() {
  const dialogWin = await openColorsDialog();
  const menulistHCM = dialogWin.document.getElementById("useDocumentColors");

  menulistHCM.doCommand();
  const newOption = dialogWin.document.getElementById("documentColorAlways");
  newOption.click();

  is(menulistHCM.value, "2", "HCM menulist should be set to always");

  await closeColorsDialog(dialogWin);

  // Verify correct initial value
  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    "a11y.theme",
    "never",
    false
  );

  gBrowser.removeCurrentTab();
});

add_task(async function testSetDefault() {
  const dialogWin = await openColorsDialog();
  const menulistHCM = dialogWin.document.getElementById("useDocumentColors");

  menulistHCM.doCommand();
  const newOption = dialogWin.document.getElementById("documentColorAutomatic");
  newOption.click();

  is(menulistHCM.value, "0", "HCM menulist should be set to default");

  await closeColorsDialog(dialogWin);

  // Verify correct initial value
  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    "a11y.theme",
    "default",
    false
  );

  gBrowser.removeCurrentTab();
});

add_task(async function testSetNever() {
  const dialogWin = await openColorsDialog();
  const menulistHCM = dialogWin.document.getElementById("useDocumentColors");

  menulistHCM.doCommand();
  const newOption = dialogWin.document.getElementById("documentColorNever");
  newOption.click();

  is(menulistHCM.value, "1", "HCM menulist should be set to never");

  await closeColorsDialog(dialogWin);

  // Verify correct initial value
  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    "a11y.theme",
    "always",
    false
  );

  gBrowser.removeCurrentTab();
});

// XXX We're uable to assert scalars that are false using assertScalar util,
// so we directly query the scalar in the scalar list below. (bug 1633883)

add_task(async function testBackplate() {
  is(
    Services.prefs.getBoolPref("browser.display.permit_backplate"),
    true,
    "Backplate is init'd to true"
  );

  Services.prefs.setBoolPref("browser.display.permit_backplate", false);
  // Verify correct recorded value
  verifyBackplate(false);

  Services.prefs.setBoolPref("browser.display.permit_backplate", true);
  // Verify correct recorded value
  verifyBackplate(true);
});
