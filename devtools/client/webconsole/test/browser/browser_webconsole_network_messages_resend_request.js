/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,Test that 'Resend Request' context menu " +
  "item resends the selected request and select it in netmonitor panel.";

const TEST_FILE = "test-network-request.html";
const TEST_PATH =
  "http://example.com/browser/devtools/client/webconsole/" + "test/browser/";

add_task(async function task() {
  await pushPref("devtools.webconsole.filter.net", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  const documentUrl = TEST_PATH + TEST_FILE;
  await loadDocument(documentUrl);
  info("Document loaded.");

  await resendNetworkRequest(hud, documentUrl);
});

/**
 * Resends a network request logged in the webconsole
 *
 * @param {Object} hud
 * @param {String} url
 *        URL of the request as logged in the netmonitor.
 */
async function resendNetworkRequest(hud, url) {
  const message = await waitFor(() => findMessage(hud, url));

  const menuPopup = await openContextMenu(hud, message);
  const openResendRequestMenuItem = menuPopup.querySelector(
    "#console-menu-resend-network-request"
  );
  ok(openResendRequestMenuItem, "resend network request item is enabled");

  // Wait for message containing the resent request url
  const onNewRequestMessage = waitForMessage(hud, url);
  openResendRequestMenuItem.click();
  await onNewRequestMessage;

  ok(true, "The resent request url is correct.");
}
