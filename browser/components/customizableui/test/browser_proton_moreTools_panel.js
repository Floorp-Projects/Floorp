/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineLazyGetter(this, "DevToolsStartup", () => {
  return Cc["@mozilla.org/devtools/startup-clh;1"].getService(
    Ci.nsICommandLineHandler
  ).wrappedJSObject;
});

// Test activating the developer button shows the More Tools panel.
add_task(async function testDevToolsPanelInToolbar() {
  // We need to force DevToolsStartup to rebuild the developer tool toggle so that
  // proton prefs are applied to the new browser window for this test.
  DevToolsStartup.developerToggleCreated = false;
  CustomizableUI.destroyWidget("developer-button");

  const win = await BrowserTestUtils.openNewBrowserWindow();

  CustomizableUI.addWidgetToArea(
    "developer-button",
    CustomizableUI.AREA_NAVBAR
  );

  // Test the developer tools panel is showing.
  let button = document.getElementById("developer-button");
  let devToolsView = PanelMultiView.getViewNode(
    document,
    "PanelUI-developer-tools"
  );
  let devToolsShownPromise = BrowserTestUtils.waitForEvent(
    devToolsView,
    "ViewShown"
  );

  EventUtils.synthesizeMouseAtCenter(button, {});
  await devToolsShownPromise;
  ok(true, "Dev Tools view is showing");
  is(
    devToolsView.children.length,
    1,
    "Dev tools subview is the only child of panel"
  );
  is(
    devToolsView.children[0].id,
    "PanelUI-developer-tools-view",
    "Dev tools child has correct id"
  );

  // Cleanup
  await BrowserTestUtils.closeWindow(win);
  CustomizableUI.reset();
});
