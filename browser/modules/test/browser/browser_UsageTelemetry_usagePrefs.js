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

/**
 * Tests that browser.engagement.pageAction-panel-*.used.count is incremented
 * when user clicks on certain page action buttons.
 */
add_task(async function test_usage_pageAction_buttons() {
  // Skip the test if proton is enabled, since these buttons are hidden.
  if (gProton) {
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [["extensions.screenshots.disabled", false]],
  });

  const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
  const sandbox = sinon.createSandbox();
  registerCleanupFunction(sandbox.restore);
  sandbox.stub(BrowserPageActions, "doCommandForAction").callsFake(() => {});

  await BrowserTestUtils.withNewTab("http://example.com/", async browser => {
    info("Open the pageAction panel");
    await promiseOpenPageActionPanel();

    function click(button) {
      info(`Click on ${button.id}`);
      EventUtils.synthesizeMouseAtCenter(button, {}, window);
    }

    let ids = [
      "pageAction-panel-copyURL",
      "pageAction-panel-emailLink",
      "pageAction-panel-pinTab",
      "pageAction-panel-screenshots_mozilla_org",
    ];
    if (
      AppConstants.platform == "macosx" ||
      AppConstants.isPlatformAndVersionAtLeast("win", "6.4")
    ) {
      ids.push("pageAction-panel-shareURL");
    }
    for (let id of ids) {
      let pref = `browser.engagement.${id}.used-count`;
      let button = document.getElementById(id);
      Assert.ok(button, `The button "${id}" should be present`);
      Services.prefs.clearUserPref(pref);
      Assert.equal(Services.prefs.getIntPref(pref, 0), 0, "Check initial");
      click(button);
      Assert.equal(Services.prefs.getIntPref(pref, 0), 1, "Check increment");
      click(button);
      Assert.equal(Services.prefs.getIntPref(pref, 0), 2, "Check increment");
      Services.prefs.clearUserPref(pref);
    }

    let hidePromise = promisePageActionPanelHidden();
    BrowserPageActions.panelNode.hidePopup();
    await hidePromise;
  });

  await SpecialPowers.popPrefEnv();
});
