/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html>Test that 'Resend Request' context menu " +
  "item resends the selected request and select it in netmonitor panel.";

const TEST_FILE = "test-network-request.html";
const TEST_PATH =
  "https://example.com/browser/devtools/client/webconsole/test/browser/";

registerCleanupFunction(async function () {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
      resolve()
    );
  });
});

add_task(async function task() {
  await pushPref("devtools.webconsole.filter.net", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  const documentUrl = TEST_PATH + TEST_FILE;
  await navigateTo(documentUrl);
  info("Document loaded.");

  const message = await waitFor(() =>
    findMessageByType(hud, documentUrl, ".network")
  );

  const menuPopup = await openContextMenu(hud, message);
  const openResendRequestMenuItem = menuPopup.querySelector(
    "#console-menu-resend-network-request"
  );
  ok(openResendRequestMenuItem, "resend network request item is enabled");

  // Wait for message containing the resent request url
  menuPopup.activateItem(openResendRequestMenuItem);
  await waitFor(
    () => findMessagesByType(hud, documentUrl, ".network").length === 2
  );

  ok(true, "The resent request url is correct.");
});
