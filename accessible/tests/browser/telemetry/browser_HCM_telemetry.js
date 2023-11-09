/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/preferences/tests/head.js",
  this
);

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

registerCleanupFunction(() => {
  reset();
});

function reset() {
  // This (manually) runs after every task in this test suite.
  // We have to add this in because the initial state of
  // `document_color_use` affects the initial state of
  // `foreground_color`/`background_color` which can change our
  // starting telem samples. This ensures each tasks makes no lasting
  // state changes.
  Services.prefs.clearUserPref("browser.display.document_color_use");
  Services.prefs.clearUserPref("browser.display.permit_backplate");
  Services.prefs.clearUserPref("browser.display.use_system_colors");
  Services.prefs.clearUserPref("layout.css.always_underline_links");
  Services.telemetry.clearEvents();
  TelemetryTestUtils.assertNumberOfEvents(0);
  Services.prefs.clearUserPref("browser.display.foreground_color");
  Services.prefs.clearUserPref("browser.display.background_color");
}

async function openColorsDialog() {
  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  const colorsButton =
    gBrowser.selectedBrowser.contentDocument.getElementById("colors");

  const dialogOpened = promiseLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/colors.xhtml"
  );
  colorsButton.doCommand();

  return dialogOpened;
}

async function closeColorsDialog(dialogWin) {
  const dialogClosed = BrowserTestUtils.waitForEvent(dialogWin, "unload");
  const button = dialogWin.document
    .getElementById("ColorsDialog")
    .getButton("accept");
  button.focus();
  button.doCommand();
  return dialogClosed;
}

function verifyBackplate(expectedValue) {
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent", false, true),
    "a11y.backplate",
    expectedValue,
    "Backplate scalar is logged as " + expectedValue
  );
}

function verifyUseSystemColors(expectedValue) {
  const snapshot = TelemetryTestUtils.getProcessScalars("parent", false, false);
  ok("a11y.use_system_colors" in snapshot, "System color usage was logged.");
  TelemetryTestUtils.assertScalar(
    snapshot,
    "a11y.use_system_colors",
    expectedValue,
    "System colors scalar is logged as " + expectedValue
  );
}

async function verifyAlwaysUnderlineLinks(expectedValue) {
  let snapshot = TelemetryTestUtils.getProcessScalars("parent", false, false);
  ok(
    "a11y.always_underline_links" in snapshot,
    "Always underline links was logged."
  );
  await TestUtils.waitForCondition(() => {
    snapshot = TelemetryTestUtils.getProcessScalars("parent", false, false);
    return snapshot["a11y.always_underline_links"] == expectedValue;
  }, "Always underline links has expected value " + expectedValue);
}

// The magic numbers below are the uint32_t values representing RGB white
// and RGB black respectively. They're directly captured as nsColors and
// follow the same bit-shift pattern.
function testIsWhite(pref, snapshot) {
  ok(pref in snapshot, "Scalar must be present.");
  is(snapshot[pref], 4294967295, "Scalar is logged as white");
}

function testIsBlack(pref, snapshot) {
  ok(pref in snapshot, "Scalar must be present.");
  is(snapshot[pref], 4278190080, "Scalar is logged as black");
}

async function setForegroundColor(color) {
  // Note: we set the foreground and background colors by modifying this pref
  // instead of setting the value attribute on the color input direclty.
  // This is because setting the value of the input with setAttribute
  // doesn't generate the correct event to save the new value to the prefs
  // store, so we have to do it ourselves.
  Services.prefs.setStringPref("browser.display.foreground_color", color);
}

async function setBackgroundColor(color) {
  Services.prefs.setStringPref("browser.display.background_color", color);
}

add_task(async function testInit() {
  const dialogWin = await openColorsDialog();
  const menulistHCM = dialogWin.document.getElementById("useDocumentColors");
  if (AppConstants.platform == "win") {
    is(
      Services.prefs.getBoolPref("browser.display.use_system_colors"),
      true,
      "Use system colours pref is init'd correctly"
    );
    verifyUseSystemColors(true);

    is(
      menulistHCM.value,
      "0",
      "HCM menulist should be set to only with HCM theme on startup for windows"
    );

    // Verify correct default value
    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true, true),
      "a11y.theme",
      "default",
      false
    );
  } else {
    is(
      Services.prefs.getBoolPref("browser.display.use_system_colors"),
      false,
      "Use system colours pref is init'd correctly"
    );
    verifyUseSystemColors(false);

    is(
      menulistHCM.value,
      "1",
      "HCM menulist should be set to never on startup for non-windows platforms"
    );

    // Verify correct default value
    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true, true),
      "a11y.theme",
      "always",
      false
    );

    await closeColorsDialog(dialogWin);

    // We should not have logged any colors
    let snapshot = TelemetryTestUtils.getProcessScalars("parent", false, true);
    ok(
      !("a11y.HCM_foreground" in snapshot),
      "Foreground color shouldn't be present."
    );
    ok(
      !("a11y.HCM_background" in snapshot),
      "Background color shouldn't be present."
    );

    // If we change the colors, our probes should not be updated
    await setForegroundColor("#ffffff"); // white
    await setBackgroundColor("#000000"); // black

    snapshot = TelemetryTestUtils.getProcessScalars("parent", false, true);
    ok(
      !("a11y.HCM_foreground" in snapshot),
      "Foreground color shouldn't be present."
    );
    ok(
      !("a11y.HCM_background" in snapshot),
      "Background color shouldn't be present."
    );
  }

  reset();
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
  let snapshot = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(snapshot, "a11y.theme", "never", false);

  snapshot = TelemetryTestUtils.getProcessScalars("parent", false, true);
  // We should have logged the default foreground and background colors
  testIsWhite("a11y.HCM_background", snapshot);
  testIsBlack("a11y.HCM_foreground", snapshot);

  // If we change the colors, our probes update on non-windows platforms.
  // On windows, useSystemColors is on by default, and so the values we set here
  // will not be written to our telemetry probes, because they capture
  // used colors, not the values of browser.foreground/background_color directly.

  setBackgroundColor("#000000");
  snapshot = TelemetryTestUtils.getProcessScalars("parent", false, true);
  if (AppConstants.platform == "win") {
    testIsWhite("a11y.HCM_background", snapshot);
  } else {
    testIsBlack("a11y.HCM_background", snapshot);
  }

  setForegroundColor("#ffffff");
  snapshot = TelemetryTestUtils.getProcessScalars("parent", false, true);
  if (AppConstants.platform == "win") {
    testIsBlack("a11y.HCM_foreground", snapshot);
  } else {
    testIsWhite("a11y.HCM_foreground", snapshot);
  }

  reset();
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

  // We should not have logged any colors
  let snapshot = TelemetryTestUtils.getProcessScalars("parent", false, true);
  ok(
    !("a11y.HCM_foreground" in snapshot),
    "Foreground color shouldn't be present."
  );
  ok(
    !("a11y.HCM_background" in snapshot),
    "Background color shouldn't be present."
  );

  // If we change the colors, our probes should not be updated anywhere
  await setForegroundColor("#ffffff"); // white
  await setBackgroundColor("#000000"); // black

  snapshot = TelemetryTestUtils.getProcessScalars("parent", false, true);
  ok(
    !("a11y.HCM_foreground" in snapshot),
    "Foreground color shouldn't be present."
  );
  ok(
    !("a11y.HCM_background" in snapshot),
    "Background color shouldn't be present."
  );

  reset();
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

  // We should not have logged any colors
  let snapshot = TelemetryTestUtils.getProcessScalars("parent", false, true);
  ok(
    !("a11y.HCM_foreground" in snapshot),
    "Foreground color shouldn't be present."
  );
  ok(
    !("a11y.HCM_background" in snapshot),
    "Background color shouldn't be present."
  );

  // If we change the colors, our probes should not be updated anywhere
  await setForegroundColor("#ffffff"); // white
  await setBackgroundColor("#000000"); // black

  snapshot = TelemetryTestUtils.getProcessScalars("parent", false, true);
  ok(
    !("a11y.HCM_foreground" in snapshot),
    "Foreground color shouldn't be present."
  );
  ok(
    !("a11y.HCM_background" in snapshot),
    "Background color shouldn't be present."
  );

  reset();
  gBrowser.removeCurrentTab();
});

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

add_task(async function testSystemColors() {
  let expectedInitVal = false;
  if (AppConstants.platform == "win") {
    expectedInitVal = true;
  }

  const dialogWin = await openColorsDialog();
  const checkbox = dialogWin.document.getElementById("browserUseSystemColors");
  checkbox.click();

  is(
    checkbox.checked,
    !expectedInitVal,
    "System colors checkbox should be modified"
  );

  await closeColorsDialog(dialogWin);

  verifyUseSystemColors(!expectedInitVal);

  reset();
  gBrowser.removeCurrentTab();
});

add_task(async function testAlwaysUnderlineLinks() {
  const expectedInitVal = false;
  await verifyAlwaysUnderlineLinks(expectedInitVal);
  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  const checkbox = gBrowser.selectedBrowser.contentDocument.getElementById(
    "alwaysUnderlineLinks"
  );
  is(
    checkbox.checked,
    expectedInitVal,
    "Always underline links checkbox has correct initial state"
  );
  checkbox.click();

  is(
    checkbox.checked,
    !expectedInitVal,
    "Always underline links checkbox should be modified"
  );
  is(
    Services.prefs.getBoolPref("layout.css.always_underline_links"),
    !expectedInitVal,
    "Always underline links pref reflects new value."
  );
  await verifyAlwaysUnderlineLinks(!expectedInitVal);
  reset();
  gBrowser.removeCurrentTab();
});
