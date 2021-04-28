/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyGetter(this, "DevToolsStartup", () => {
  return Cc["@mozilla.org/devtools/startup-clh;1"].getService(
    Ci.nsICommandLineHandler
  ).wrappedJSObject;
});

// Test activating the developer button shows the More Tools panel.
add_task(async function testMoreToolsPanelInToolbar() {
  // We need to force DevToolsStartup to rebuild the developer tool toggle so that
  // proton prefs are applied to the new browser window for this test.
  DevToolsStartup.developerToggleCreated = false;
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.proton.enabled", true],
      ["browser.proton.doorhangers.enabled", true],
    ],
  });
  CustomizableUI.destroyWidget("developer-button");

  // Now open a new window where the proton prefs will be applied.
  const win = await BrowserTestUtils.openNewBrowserWindow();

  CustomizableUI.addWidgetToArea(
    "developer-button",
    CustomizableUI.AREA_NAVBAR
  );

  // Test the "More Tools" panel is showing.
  let button = document.getElementById("developer-button");
  let moreToolsView = PanelMultiView.getViewNode(document, "appmenu-moreTools");
  let moreToolsShownPromise = BrowserTestUtils.waitForEvent(
    moreToolsView,
    "ViewShown"
  );

  EventUtils.synthesizeMouseAtCenter(button, {});
  await moreToolsShownPromise;
  ok(true, "More Tools view is showing");

  // Cleanup
  await BrowserTestUtils.closeWindow(win);
  CustomizableUI.reset();
});
