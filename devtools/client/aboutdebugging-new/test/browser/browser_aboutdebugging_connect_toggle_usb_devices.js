/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");
const { adbProcess } = require("devtools/shared/adb/adb-process");

/**
 * Check that USB Devices scanning can be enabled and disabled from the connect page.
 */
add_task(async function() {
  await pushPref("devtools.remote.adb.extensionURL",
                 CHROME_URL_ROOT + "resources/test-adb-extension/adb-extension-#OS#.xpi");

  const { document, tab } = await openAboutDebugging();

  await selectConnectPage(document);

  info("Wait until Connect page is displayed");
  await waitUntil(() => document.querySelector(".js-connect-page"));

  info("Check that by default USB devices are disabled");
  const usbDisabledMessage = document.querySelector(".js-connect-usb-disabled-message");
  ok(usbDisabledMessage, "A message about enabling USB devices is rendered");

  const usbToggleButton = document.querySelector(".js-connect-usb-toggle-button");
  ok(usbToggleButton, "The button to toggle USB devices debugging is rendered");
  ok(usbToggleButton.textContent.includes("Enable"),
    "The text of the toggle USB button is correct");

  info("Click on the toggle button");
  usbToggleButton.click();

  info("Wait until the toggle button text is updated");
  await waitUntil(() => usbToggleButton.textContent.includes("Disable"));
  ok(!document.querySelector(".js-connect-usb-disabled-message"),
    "The message about enabling USB devices is no longer rendered");

  info("Check that the addon was installed with the proper source");
  const adbExtensionId = Services.prefs.getCharPref("devtools.remote.adb.extensionID");
  const addon = await AddonManager.getAddonByID(adbExtensionId);
  Assert.deepEqual(addon.installTelemetryInfo, { source: "about:debugging" },
    "Got the expected addon.installTelemetryInfo");

  // Right now we are resuming as soon as "USB devices enabled" is displayed, but ADB
  // might still be starting up. If we move to uninstall directly, the ADB startup will
  // fail and we will have an unhandled promise rejection.
  // See Bug 1498469.
  info("Wait until ADB has started.");
  await waitUntil(() => adbProcess.ready);

  info("Click on the toggle button");
  usbToggleButton.click();

  info("Wait until the toggle button text is updated");
  await waitUntil(() => usbToggleButton.textContent.includes("Enable"));
  ok(document.querySelector(".js-connect-usb-disabled-message"),
    "The message about enabling USB devices is rendered again");

  await removeTab(tab);
});
