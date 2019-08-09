/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that JS errors and CSS warnings open view source when their source link
// is clicked in the Browser Console.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<p>hello world" +
  "<button onclick='foobar.explode()'>click!</button>";

add_task(async function() {
  await pushPref("devtools.browserconsole.contentMessages", true);
  await addTab(TEST_URI);

  const hud = await BrowserConsoleManager.toggleBrowserConsole();
  ok(hud, "browser console opened");

  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  info("generate exception and wait for the message");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    const button = content.document.querySelector("button");
    button.click();
  });

  await waitForMessageAndViewSource(
    hud,
    "ReferenceError: foobar is not defined"
  );
});

async function waitForMessageAndViewSource(hud, message) {
  const msg = await waitFor(() => findMessage(hud, message));
  ok(msg, `Message found: "${message}"`);

  const locationNode = msg.querySelector(
    ".message-location .frame-link-source"
  );
  ok(locationNode, "Message location link element found");

  const onTabOpen = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
  locationNode.click();
  const newTab = await onTabOpen;
  ok(true, "The view source tab was opened in response to clicking the link");
  BrowserTestUtils.removeTab(newTab);
}
