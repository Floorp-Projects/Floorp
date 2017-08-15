"use strict";

/**
 * Check that opening customize mode in a popup opens it in the main window.
 */
add_task(async function open_customize_mode_from_popup() {
  let promiseWindow = BrowserTestUtils.waitForNewWindow(true);
  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.window.open("about:blank", "_blank", "height=300,toolbar=no");
  });
  let win = await promiseWindow;
  let customizePromise = BrowserTestUtils.waitForEvent(gNavToolbox, "customizationready");
  win.gCustomizeMode.enter();
  await customizePromise;
  ok(document.documentElement.hasAttribute("customizing"),
     "Should have opened customize mode in the parent window");
  await endCustomizing();
  await BrowserTestUtils.closeWindow(win);
});
