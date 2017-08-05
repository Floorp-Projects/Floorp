/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PREF_UI_DENSITY = "browser.uidensity";
const PREF_AUTO_TOUCH_MODE = "browser.touchmode.auto";

async function testModeMenuitem(mode, modePref) {
  await startCustomizing();

  let win = document.getElementById("main-window");
  let popupButton = document.getElementById("customization-uidensity-button");
  let popup = document.getElementById("customization-uidensity-menu");

  // Show the popup.
  let popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(popupButton, {});
  await popupShownPromise;

  let item = document.getElementById("customization-uidensity-menuitem-" + mode);
  let normalItem = document.getElementById("customization-uidensity-menuitem-normal");

  is(normalItem.getAttribute("active"), "true",
     "Normal mode menuitem should be active by default");

  // Hover over the mode menuitem and wait until the UI density is updated.
  EventUtils.synthesizeMouseAtCenter(item, { type: "mouseover" });
  await BrowserTestUtils.waitForAttribute("uidensity", win, mode);

  is(win.getAttribute("uidensity"), mode,
     `UI Density should be set to ${mode} on ${mode} menuitem hover`);

  is(Services.prefs.getIntPref(PREF_UI_DENSITY), window.gUIDensity.MODE_NORMAL,
     `UI Density pref should still be set to normal on ${mode} menuitem hover`);

  // Hover the normal menuitem again and check that the UI density reset to normal.
  EventUtils.synthesizeMouseAtCenter(normalItem, { type: "mouseover" });
  await BrowserTestUtils.waitForCondition(() => !win.hasAttribute("uidensity"));

  ok(!win.hasAttribute("uidensity"),
     `UI Density should be reset when no longer hovering the ${mode} menuitem`);

  // Select the custom UI density and wait for the popup to be hidden.
  let popupHiddenPromise = popupHidden(popup);
  EventUtils.synthesizeMouseAtCenter(item, {});
  await popupHiddenPromise;

  // Check that the click permanently changed the UI density.
  is(win.getAttribute("uidensity"), mode,
     `UI Density should be set to ${mode} on ${mode} menuitem click`);
  is(Services.prefs.getIntPref(PREF_UI_DENSITY), modePref,
     `UI Density pref should be set to ${mode} when clicking the ${mode} menuitem`);

  // Open the popup again.
  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(popupButton, {});
  await popupShownPromise;

  // Check that the menuitem is still active after opening and closing the popup.
  is(item.getAttribute("active"), "true", `${mode} mode menuitem should be active`);

  // Hide the popup again.
  popupHiddenPromise = popupHidden(popup);
  EventUtils.synthesizeMouseAtCenter(popupButton, {});
  await popupHiddenPromise;

  // Check that the menuitem is still active after re-opening customize mode.
  await endCustomizing();
  await startCustomizing();

  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(popupButton, {});
  await popupShownPromise;

  is(item.getAttribute("active"), "true",
     `${mode} mode menuitem should be active after entering and exiting customize mode`);

  // Click the normal menuitem and check that the density is reset.
  popupHiddenPromise = popupHidden(popup);
  EventUtils.synthesizeMouseAtCenter(normalItem, {});
  await popupHiddenPromise;

  ok(!win.hasAttribute("uidensity"),
     "UI Density should be reset when clicking the normal menuitem");

  is(Services.prefs.getIntPref(PREF_UI_DENSITY), window.gUIDensity.MODE_NORMAL,
     "UI Density pref should be set to normal.");

  // Show the popup and click on the mode menuitem again to test the
  // reset default feature.
  popupShownPromise = popupShown(popup);
  EventUtils.synthesizeMouseAtCenter(popupButton, {});
  await popupShownPromise;

  popupHiddenPromise = popupHidden(popup);
  EventUtils.synthesizeMouseAtCenter(item, {});
  await popupHiddenPromise;

  is(win.getAttribute("uidensity"), mode,
     `UI Density should be set to ${mode} on ${mode} menuitem click`);

  is(Services.prefs.getIntPref(PREF_UI_DENSITY), modePref,
     `UI Density pref should be set to ${mode} when clicking the ${mode} menuitem`);

  await gCustomizeMode.reset();

  ok(!win.hasAttribute("uidensity"),
     "UI Density should be reset when clicking the normal menuitem");

  is(Services.prefs.getIntPref(PREF_UI_DENSITY), window.gUIDensity.MODE_NORMAL,
     "UI Density pref should be set to normal.");

  await endCustomizing();
}

add_task(async function test_compact_mode_menuitem() {
  if (!AppConstants.MOZ_PHOTON_THEME) {
    ok(true, "Skipping test because Photon is not enabled.");
    return;
  }

  await testModeMenuitem("compact", window.gUIDensity.MODE_COMPACT);
});

add_task(async function test_touch_mode_menuitem() {
  if (!AppConstants.MOZ_PHOTON_THEME) {
    ok(true, "Skipping test because Photon is not enabled.");
    return;
  }

  // OSX doesn't get touch mode for now.
  if (AppConstants.platform == "macosx") {
    is(document.getElementById("customization-uidensity-menuitem-touch"), null,
       "There's no touch option on Mac OSX");
    return;
  }

  await testModeMenuitem("touch", window.gUIDensity.MODE_TOUCH);

  // Test the checkbox for automatic Touch Mode transition
  // in Windows 10 Tablet Mode.
  if (AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
    await startCustomizing();

    let popupButton = document.getElementById("customization-uidensity-button");
    let popup = document.getElementById("customization-uidensity-menu");
    let popupShownPromise = popupShown(popup);
    EventUtils.synthesizeMouseAtCenter(popupButton, {});
    await popupShownPromise;

    let checkbox = document.getElementById("customization-uidensity-autotouchmode-checkbox");
    ok(checkbox.checked, "Checkbox should be checked by default");

    // Test toggling the checkbox.
    EventUtils.synthesizeMouseAtCenter(checkbox, {});
    is(Services.prefs.getBoolPref(PREF_AUTO_TOUCH_MODE), false,
       "Automatic Touch Mode is off when the checkbox is unchecked.");

    EventUtils.synthesizeMouseAtCenter(checkbox, {});
    is(Services.prefs.getBoolPref(PREF_AUTO_TOUCH_MODE), true,
       "Automatic Touch Mode is on when the checkbox is checked.");

    // Test reset to defaults.
    EventUtils.synthesizeMouseAtCenter(checkbox, {});
    is(Services.prefs.getBoolPref(PREF_AUTO_TOUCH_MODE), false,
       "Automatic Touch Mode is off when the checkbox is unchecked.");

    await gCustomizeMode.reset();
    is(Services.prefs.getBoolPref(PREF_AUTO_TOUCH_MODE), true,
       "Automatic Touch Mode is on when the checkbox is checked.");
  }
});

add_task(async function cleanup() {
  await endCustomizing();

  Services.prefs.clearUserPref(PREF_UI_DENSITY);
  Services.prefs.clearUserPref(PREF_AUTO_TOUCH_MODE);
});
