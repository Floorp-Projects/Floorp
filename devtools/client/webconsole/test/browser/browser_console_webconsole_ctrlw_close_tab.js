/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that Ctrl-W closes the Browser Console and that Ctrl-W closes the
// current tab when using the Web Console - bug 871156.

"use strict";

add_task(async function () {
  const TEST_URI =
    "data:text/html;charset=utf8,<!DOCTYPE html><title>bug871156</title>\n" +
    "<p>hello world";
  const firstTab = gBrowser.selectedTab;

  let hud = await openNewTabAndConsole(TEST_URI);

  const toolbox = gDevTools.getToolboxForTab(gBrowser.selectedTab);

  const tabClosed = once(gBrowser.tabContainer, "TabClose");
  tabClosed.then(() => info("tab closed"));

  const tabSelected = new Promise(resolve => {
    gBrowser.tabContainer.addEventListener(
      "TabSelect",
      function () {
        if (gBrowser.selectedTab == firstTab) {
          info("tab selected");
          resolve(null);
        }
      },
      { once: true }
    );
  });

  const toolboxDestroyed = toolbox.once("destroyed", () => {
    info("toolbox destroyed");
  });

  // Get out of the web console initialization.
  executeSoon(() => {
    EventUtils.synthesizeKey("w", { accelKey: true });
  });

  await Promise.all([tabClosed, toolboxDestroyed, tabSelected]);
  info("Promise.all resolved. start testing the Browser Console");

  hud = await BrowserConsoleManager.toggleBrowserConsole();
  ok(hud, "Browser Console opened");

  const onBrowserConsoleClosed = new Promise(resolve => {
    Services.obs.addObserver(function onDestroy() {
      Services.obs.removeObserver(onDestroy, "web-console-destroyed");
      resolve();
    }, "web-console-destroyed");
  });

  await waitForAllTargetsToBeAttached(hud.commands.targetCommand);
  waitForFocus(() => {
    EventUtils.synthesizeKey("w", { accelKey: true }, hud.iframeWindow);
  }, hud.iframeWindow);

  await onBrowserConsoleClosed;
  ok(true, "the Browser Console closed");
});
