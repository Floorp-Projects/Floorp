/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-adb.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-adb.js", this);

const { adbAddon } = require("devtools/shared/adb/adb-addon");

/**
 * This test asserts that the sidebar shows a message describing the status of the USB
 * devices scanning.
 */
add_task(async function() {
  await pushPref("devtools.remote.adb.extensionURL",
                 CHROME_URL_ROOT + "resources/test-adb-extension/adb-extension-#OS#.xpi");
  await checkAdbNotRunning();

  const { document, tab } = await openAboutDebugging();

  const usbStatusElement = document.querySelector(".js-sidebar-usb-status");
  ok(usbStatusElement, "Sidebar shows the USB status element");
  ok(usbStatusElement.textContent.includes("USB disabled"),
    "USB status element has the expected content");

  info("Install the adb extension and wait for the message to udpate");
  // Use "internal" as the install source to avoid triggering telemetry.
  adbAddon.install("internal");
  await waitUntil(() => usbStatusElement.textContent.includes("USB enabled"));
  // Right now we are resuming as soon as "USB enabled" is displayed, but ADB
  // might still be starting up. If we move to uninstall directly, the ADB startup will
  // fail and we will have an unhandled promise rejection.
  // See Bug 1498469.
  await waitForAdbStart();

  info("Uninstall the adb extension and wait for the message to udpate");
  adbAddon.uninstall();
  await waitUntil(() => usbStatusElement.textContent.includes("USB disabled"));
  await stopAdbProcess();

  await removeTab(tab);
});
