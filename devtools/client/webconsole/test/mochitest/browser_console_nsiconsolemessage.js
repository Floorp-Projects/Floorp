/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that nsIConsoleMessages are displayed in the Browser Console.

"use strict";

const TEST_URI =
`data:text/html;charset=utf8,
<title>browser_console_nsiconsolemessage.js</title>
<p>hello world<p>
nsIConsoleMessages ftw!`;

add_task(async function() {
  // We don't use `openNewTabAndConsole()` here because we need to log a message
  // before opening the web console.
  await addTab(TEST_URI);

  // Test for cached nsIConsoleMessages.
  Services.console.logStringMessage("cachedBrowserConsoleMessage");

  info("open web console");
  let hud = await openConsole();

  ok(hud, "web console opened");

  // This "liveBrowserConsoleMessage" message should not be displayed.
  Services.console.logStringMessage("liveBrowserConsoleMessage");

  // Log a "foobarz" message so that we can be certain the previous message is
  // not displayed.
  let text = "foobarz";
  const onFooBarzMessage = waitForMessage(hud, text);
  ContentTask.spawn(gBrowser.selectedBrowser, text, function(msg) {
    content.console.log(msg);
  });
  await onFooBarzMessage;
  ok(true, `"${text}" log is displayed in the Web Console as expected`);

  // Ensure the "liveBrowserConsoleMessage" and "cachedBrowserConsoleMessage"
  // messages are not displayed.
  text = hud.outputNode.textContent;
  ok(!text.includes("cachedBrowserConsoleMessage"),
    "cached nsIConsoleMessages are not displayed");
  ok(!text.includes("liveBrowserConsoleMessage"),
    "nsIConsoleMessages are not displayed");

  await closeConsole();

  info("web console closed");
  hud = await HUDService.toggleBrowserConsole();
  ok(hud, "browser console opened");

  await waitFor(() => findMessage(hud, "cachedBrowserConsoleMessage"));
  Services.console.logStringMessage("liveBrowserConsoleMessage2");
  await waitFor(() => findMessage(hud, "liveBrowserConsoleMessage2"));

  const msg = await waitFor(() => findMessage(hud, "liveBrowserConsoleMessage"));
  ok(msg, "message element for liveBrowserConsoleMessage (nsIConsoleMessage)");

  // Disable the log filter.
  await setFilterState(hud, {
    log: false
  });

  // And then checking that the log messages are hidden.
  await waitFor(() => findMessages(hud, "cachedBrowserConsoleMessage").length === 0);
  await waitFor(() => findMessages(hud, "liveBrowserConsoleMessage").length === 0);
  await waitFor(() => findMessages(hud, "liveBrowserConsoleMessage2").length === 0);

  resetFilters(hud);
  await setFilterBarVisible(hud, false);
});
