/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function setup() {
  await setupPolicyEngineWithJson({
    policies: {
      TranslateEnabled: false,
    },
  });
});

add_task(async function test_translate_pref_disabled() {
  is(
    Services.prefs.getBoolPref("browser.translations.enable"),
    false,
    "The translations pref should be disabled when the enterprise policy is active."
  );
});

add_task(async function test_translate_button_disabled() {
  // Since testing will apply the policy after the browser has already started,
  // we will need to open a new window to actually see changes from the policy
  let win = await BrowserTestUtils.openNewBrowserWindow();

  let appMenuButton = win.document.getElementById("PanelUI-menu-button");
  let viewShown = BrowserTestUtils.waitForEvent(
    win.PanelUI.mainView,
    "ViewShown"
  );

  appMenuButton.click();
  await viewShown;

  let translateSiteButton = win.document.getElementById(
    "appMenu-translate-button"
  );

  is(
    translateSiteButton.hidden,
    true,
    "The app-menu translate button should be hidden when the enterprise policy is active."
  );

  is(
    translateSiteButton.disabled,
    true,
    "The app-menu translate button should be disabled when the enterprise policy is active."
  );

  await BrowserTestUtils.closeWindow(win);
});
