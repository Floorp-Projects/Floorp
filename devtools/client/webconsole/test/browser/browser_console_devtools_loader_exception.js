/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that exceptions from scripts loaded with the DevTools loader are
// opened correctly in View Source from the Browser Console.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<p>browser_console_devtools_loader_exception.js</p>";

add_task(async function() {
  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  const wcHud = await openNewTabAndConsole(TEST_URI);
  ok(wcHud, "web console opened");

  const bcHud = await BrowserConsoleManager.toggleBrowserConsole();
  ok(bcHud, "browser console opened");

  // Cause an exception in a script loaded with the DevTools loader.
  const toolbox = wcHud.toolbox;
  const oldPanels = toolbox._toolPanels;
  // non-iterable
  toolbox._toolPanels = {};

  function fixToolbox() {
    toolbox._toolPanels = oldPanels;
  }

  info("generate exception and wait for message");

  executeSoon(() => {
    expectUncaughtException();
    executeSoon(fixToolbox);
    toolbox.getToolPanels();
  });

  const msg = await waitFor(() =>
    findMessage(bcHud, "TypeError: this._toolPanels is not iterable")
  );

  fixToolbox();

  ok(msg, `Message found: "TypeError: this._toolPanels is not iterable"`);

  const locationNode = msg.querySelector(
    ".message-location .frame-link-source"
  );
  ok(locationNode, "Message location link element found");

  const url = locationNode.href;
  info("view-source url: " + url);
  ok(url, "we have some source URL after the click");
  ok(url.includes("toolbox.js"), "we have the expected view source URL");
  ok(!url.includes("->"), "no -> in the URL given to view-source");

  const isFissionEnabledForBrowserConsole = Services.prefs.getBoolPref(
    "devtools.browsertoolbox.fission",
    false
  );

  const { targetCommand } = bcHud.commands;
  // If Fission is not enabled for the Browser Console (e.g. in Beta at this moment),
  // the target list won't watch for Frame targets, and as a result we won't have issues
  // with pending connections to the server that we're observing when attaching the target.
  const onViewSourceTargetAvailable = !isFissionEnabledForBrowserConsole
    ? Promise.resolve()
    : new Promise(resolve => {
        const onAvailable = ({ targetFront }) => {
          if (targetFront.url.includes("view-source:")) {
            targetCommand.unwatchTargets(
              [targetCommand.TYPES.FRAME],
              onAvailable
            );
            resolve();
          }
        };
        targetCommand.watchTargets([targetCommand.TYPES.FRAME], onAvailable);
      });

  const onTabOpen = BrowserTestUtils.waitForNewTab(
    gBrowser,
    tabUrl => tabUrl.startsWith("view-source:"),
    true
  );
  locationNode.click();

  await onTabOpen;
  ok(true, "The view source tab was opened in response to clicking the link");

  info("Wait for the frame target to be available");
  await onViewSourceTargetAvailable;
});
