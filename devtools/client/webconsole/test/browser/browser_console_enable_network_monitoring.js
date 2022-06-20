/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that enabling/disabling network monitoring work in the browser console.
"use strict";

const TEST_IMAGE =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/test-image.png";

requestLongerTimeout(10);

// Test that "Enable Network Monitoring" work as expected in the browser
// console
add_task(async function testEnableNetworkMonitoringInBrowserConsole() {
  const hud = await BrowserConsoleManager.toggleBrowserConsole();
  const enableNetworkMonitoringSelector =
    ".webconsole-console-settings-menu-item-enableNetworkMonitoring";

  info("Set the focus on the Browser Console");
  hud.iframeWindow.focus();

  await setFilterState(hud, {
    netxhr: true,
    net: true,
  });

  info("Check that the 'Enable Network Monitoring' setting is off by default");
  await checkConsoleSettingState(hud, enableNetworkMonitoringSelector, false);

  await fetch(TEST_IMAGE);

  await checkNoMessageExists(hud, "test-image.png", ".message.network");

  info("Turn on network monitoring");
  await toggleNetworkMonitoringConsoleSetting(hud, true);

  let onMessageLogged = waitForMessageByType(
    hud,
    "test-image.png?id=1",
    ".message.network"
  );
  await fetch(TEST_IMAGE + "?id=1");
  await onMessageLogged;

  info("Turn off network monitoring");
  await toggleNetworkMonitoringConsoleSetting(hud, false);

  await fetch(TEST_IMAGE + "?id=2");

  await checkNoMessageExists(hud, "test-image.png?id=2", ".message.network");

  info("Turn on network monitoring again");
  await toggleNetworkMonitoringConsoleSetting(hud, true);

  onMessageLogged = waitForMessageByType(
    hud,
    "test-image.png?id=3",
    ".message.network"
  );
  await fetch(TEST_IMAGE + "?id=3");
  await onMessageLogged;

  info(
    "Test that the 'Enable Network Monitoring' setting is persisted across browser console reopens "
  );

  info("Close the  browser console");
  await safeCloseBrowserConsole({ clearOutput: true });
  await BrowserConsoleManager.closeBrowserConsole();

  info("Reopen the  browser console");
  const hud2 = await BrowserConsoleManager.toggleBrowserConsole();
  hud2.iframeWindow.focus();

  info("Check that the 'Enable Network Monitoring' setting is on");
  await checkConsoleSettingState(hud2, enableNetworkMonitoringSelector, true);

  onMessageLogged = waitForMessageByType(
    hud2,
    "test-image.png?id=4",
    ".message.network"
  );
  await fetch(TEST_IMAGE + "?id=4");
  await onMessageLogged;

  info("Clear and close the Browser Console");
  // Reset the network monitoring setting to off
  await toggleNetworkMonitoringConsoleSetting(hud2, false);
  await safeCloseBrowserConsole({ clearOutput: true });
});

/**
 * Check that a message is not logged.
 *
 * @param object hud
 *        The web console.
 * @param string text
 *        A substring that can be found in the message.
 * @param selector [optional]
 *        The selector to use in finding the message.
 */
async function checkNoMessageExists(hud, msg, selector) {
  info(`Checking that "${msg}" was not logged`);
  let messages;
  try {
    messages = await waitFor(async () => {
      const msgs = await findMessagesVirtualized({ hud, text: msg, selector });
      return msgs.length > 0 ? msgs : null;
    });
    ok(!messages.length, `"${msg}" was logged once`);
  } catch (e) {
    ok(true, `Message "${msg}" wasn't logged\n`);
  }
}
