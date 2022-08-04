/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that JS errors and CSS warnings open view source when their source link
// is clicked in the Browser Console.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><p>hello world" +
  "<button onclick='foobar.explode()'>click!</button>";

add_task(async function() {
  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  await pushPref("devtools.browserconsole.contentMessages", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");
  await addTab(TEST_URI);

  const hud = await BrowserConsoleManager.toggleBrowserConsole();
  ok(hud, "browser console opened");

  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  info("generate exception and wait for the message");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const button = content.document.querySelector("button");
    button.click();
  });

  const messageText = "ReferenceError: foobar is not defined";

  const msg = await waitFor(
    () => findErrorMessage(hud, messageText),
    `Message "${messageText}" wasn't found`
  );
  ok(msg, `Message found: "${messageText}"`);

  const locationNode = msg.querySelector(
    ".message-location .frame-link-source"
  );
  ok(locationNode, "Message location link element found");

  const onTabOpen = BrowserTestUtils.waitForNewTab(
    gBrowser,
    url => url.startsWith("view-source:"),
    true
  );
  locationNode.click();
  await onTabOpen;
  ok(true, "The view source tab was opened in response to clicking the link");
});
