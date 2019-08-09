/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that exceptions from scripts loaded with the DevTools loader are
// opened correctly in View Source from the Browser Console.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<p>browser_console_devtools_loader_exception.js</p>";

add_task(async function() {
  const wcHud = await openNewTabAndConsole(TEST_URI);
  ok(wcHud, "web console opened");

  const bcHud = await BrowserConsoleManager.toggleBrowserConsole();
  ok(bcHud, "browser console opened");

  // Cause an exception in a script loaded with the DevTools loader.
  const toolbox = gDevTools.getToolbox(wcHud.target);
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

  const onTabOpen = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
  locationNode.click();

  const newTab = await onTabOpen;
  ok(true, "The view source tab was opened in response to clicking the link");

  await BrowserTestUtils.removeTab(newTab);
});
