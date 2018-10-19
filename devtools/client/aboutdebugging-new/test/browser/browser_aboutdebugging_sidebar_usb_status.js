/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { adbAddon } = require("devtools/shared/adb/adb-addon");
const { ADB } = require("devtools/shared/adb/adb");

/**
 * This test asserts that the sidebar shows a message describing the status of the USB
 * devices scanning.
 */
add_task(async function() {
  await pushPref("devtools.remote.adb.extensionURL",
                 CHROME_URL_ROOT + "resources/test-adb-extension/adb-extension-#OS#.xpi");

  const { document, tab } = await openAboutDebugging();

  const usbStatusElement = document.querySelector(".js-sidebar-usb-status");
  ok(usbStatusElement, "Sidebar shows the USB status element");
  ok(usbStatusElement.textContent.includes("USB devices disabled"),
    "USB status element has the expected content");

  info("Install the adb extension and wait for the message to udpate");
  adbAddon.install();
  await waitUntil(() => usbStatusElement.textContent.includes("USB devices enabled"));

  // Right now we are resuming as soon as "USB devices enabled" is displayed, but ADB
  // might still be starting up. If we move to uninstall directly, the ADB startup will
  // fail and we will have an unhandled promise rejection.
  // See Bug 1498469.
  info("Wait until ADB has started.");
  await waitUntil(() => ADB.ready);

  info("Uninstall the adb extension and wait for the message to udpate");
  adbAddon.uninstall();
  await waitUntil(() => usbStatusElement.textContent.includes("USB devices disabled"));

  await removeTab(tab);
});
