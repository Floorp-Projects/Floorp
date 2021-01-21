/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BUTTONS_TO_TEST = {
  "downloads-button": "browser.engagement.downloads-button.has-used",
  "fxa-toolbar-menu-button":
    "browser.engagement.fxa-toolbar-menu-button.has-used",
  "home-button": "browser.engagement.home-button.has-used",
  "sidebar-button": "browser.engagement.sidebar-button.has-used",
  "library-button": "browser.engagement.library-button.has-used",
};

/**
 * Tests that preferences are set when users interact with the
 * buttons in BUTTONS_TO_TEST.
 */
add_task(async function test_usage_button_prefs_set() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.autohideButton", false]],
  });

  registerCleanupFunction(() => {
    // Clicking on the sidebar button will show the sidebar, so we'll
    // make sure it's hidden when the test ends.
    SidebarUI.hide();
  });

  const PREFS_TO_FALSE = Object.values(BUTTONS_TO_TEST).map(prefName => {
    return [prefName, false];
  });

  await SpecialPowers.pushPrefEnv({
    set: PREFS_TO_FALSE,
  });

  // We open a new tab to ensure the test passes verify on Windows
  await BrowserTestUtils.withNewTab("about:blank", () => {
    for (let buttonID in BUTTONS_TO_TEST) {
      let pref = BUTTONS_TO_TEST[buttonID];
      Assert.ok(
        !Services.prefs.getBoolPref(pref),
        `${pref} should start at false.`
      );

      info(`Clicking on ${buttonID}`);
      let element = document.getElementById(buttonID);
      EventUtils.synthesizeMouseAtCenter(element, {}, window);

      Assert.ok(
        Services.prefs.getBoolPref(pref),
        `${pref} should now be true after interacting.`
      );
    }
  });
});

/**
 * Tests that browser.engagement.ctrlTab.has-used is set when
 * user presses ctrl-tab.
 */
add_task(async function test_usage_ctrltab_pref_set() {
  let ctrlTabUsed = "browser.engagement.ctrlTab.has-used";

  await SpecialPowers.pushPrefEnv({
    set: [[ctrlTabUsed, false]],
  });

  Assert.ok(
    !Services.prefs.getBoolPref(ctrlTabUsed),
    `${ctrlTabUsed} should start at false.`
  );

  EventUtils.synthesizeKey("VK_TAB", { ctrlKey: true });

  Assert.ok(
    Services.prefs.getBoolPref(ctrlTabUsed),
    `${ctrlTabUsed} should now be true after interacting.`
  );
});
