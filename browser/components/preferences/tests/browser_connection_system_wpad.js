/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_system_wpad() {
  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  const connectionURL =
    "chrome://browser/content/preferences/dialogs/connection.xhtml";

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("network.proxy.system_wpad.allowed");
  });

  Services.prefs.setBoolPref("network.proxy.system_wpad.allowed", true);
  let dialog = await openAndLoadSubDialog(connectionURL);
  let dialogElement = dialog.document.getElementById("ConnectionsDialog");
  let systemWpad = dialog.document.getElementById("systemWpad");
  Assert.ok(!systemWpad.hidden, "Use system WPAD checkbox should be visible");
  let dialogClosingPromise = BrowserTestUtils.waitForEvent(
    dialogElement,
    "dialogclosing"
  );
  dialogElement.cancelDialog();
  await dialogClosingPromise;

  Services.prefs.setBoolPref("network.proxy.system_wpad.allowed", false);
  dialog = await openAndLoadSubDialog(connectionURL);
  dialogElement = dialog.document.getElementById("ConnectionsDialog");
  systemWpad = dialog.document.getElementById("systemWpad");
  Assert.ok(systemWpad.hidden, "Use system WPAD checkbox should be hidden");
  dialogClosingPromise = BrowserTestUtils.waitForEvent(
    dialogElement,
    "dialogclosing"
  );
  dialogElement.cancelDialog();
  await dialogClosingPromise;

  gBrowser.removeCurrentTab();
});
