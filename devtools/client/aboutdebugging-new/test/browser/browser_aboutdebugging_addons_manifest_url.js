/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { adbAddon } = require("devtools/shared/adb/adb-addon");

const ABD_ADDON_NAME = "ADB binary provider";

/* import-globals-from helper-adb.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-adb.js", this);

// Test that manifest URLs for addon targets show the manifest correctly in a new tab.
// This test reuses the ADB extension to be sure to have a valid manifest URL to open.
add_task(async function() {
  await pushPref(
    "devtools.remote.adb.extensionURL",
    CHROME_URL_ROOT + "resources/test-adb-extension/adb-extension-#OS#.xpi"
  );
  await checkAdbNotRunning();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const usbStatusElement = document.querySelector(".qa-sidebar-usb-status");

  info("Install ADB");
  adbAddon.install("internal");
  await waitUntil(() => usbStatusElement.textContent.includes("USB enabled"));
  await waitForAdbStart();

  info("Wait until the debug target for ADB appears");
  await waitUntil(() => findDebugTargetByText(ABD_ADDON_NAME, document));
  const adbExtensionItem = findDebugTargetByText(ABD_ADDON_NAME, document);

  const manifestUrlElement = adbExtensionItem.querySelector(".qa-manifest-url");
  ok(manifestUrlElement, "A link to the manifest is displayed");

  info("Click on the manifest URL and wait for the new tab to open");
  const onTabOpened = once(gBrowser.tabContainer, "TabOpen");
  manifestUrlElement.click();
  const { target } = await onTabOpened;
  await BrowserTestUtils.browserLoaded(target.linkedBrowser);

  info("Retrieve the text content of the new tab");
  const textContent = await ContentTask.spawn(
    target.linkedBrowser,
    {},
    function() {
      return content.wrappedJSObject.document.body.textContent;
    }
  );

  const manifestObject = JSON.parse(textContent);
  ok(manifestObject, "The displayed content is a valid JSON object");
  is(
    manifestObject.name,
    ABD_ADDON_NAME,
    "Manifest tab shows the expected content"
  );

  info("Close the manifest.json tab");
  await removeTab(target);

  info("Uninstall the adb extension and wait for the message to udpate");
  adbAddon.uninstall();
  await waitUntil(() => usbStatusElement.textContent.includes("USB disabled"));
  await stopAdbProcess();

  await waitForRequestsToSettle(window.AboutDebugging.store);
  await removeTab(tab);
});
