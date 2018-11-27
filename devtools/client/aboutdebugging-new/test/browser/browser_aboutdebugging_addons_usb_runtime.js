/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_ID = "test-runtime-id";
const RUNTIME_DEVICE_NAME = "test device name";
const RUNTIME_APP_NAME = "TestApp";

/* import-globals-from mocks/head-usb-mocks.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "mocks/head-usb-mocks.js", this);

// Test that addons are displayed and updated for USB runtimes when expected.
add_task(async function() {
  const usbMocks = new UsbMocks();
  usbMocks.enableMocks();
  registerCleanupFunction(() => usbMocks.disableMocks());

  const { document, tab } = await openAboutDebugging();

  const usbClient = usbMocks.createRuntime(RUNTIME_ID, {
    deviceName: RUNTIME_DEVICE_NAME,
    name: RUNTIME_APP_NAME,
  });
  usbMocks.emitUpdate();

  await connectToRuntime(RUNTIME_DEVICE_NAME, document);
  await selectRuntime(RUNTIME_DEVICE_NAME, RUNTIME_APP_NAME, document);

  const extensionPane = getDebugTargetPane("Extensions", document);
  info("Check an empty target pane message is displayed");
  ok(extensionPane.querySelector(".js-debug-target-list-empty"),
    "Extensions list is empty");

  info("Add an extension to the remote client");
  const addon = { name: "Test extension name", debuggable: true };
  usbClient.listAddons = () => [addon];
  usbClient._eventEmitter.emit("addonListChanged");

  info("Wait until the extension appears");
  await waitUntil(() => !extensionPane.querySelector(".js-debug-target-list-empty"));

  const extensionTarget = findDebugTargetByText("Test extension name", document);
  ok(extensionTarget, "Extension target appeared for the USB runtime");

  // The goal here is to check that USB runtimes addons are only updated when the USB
  // runtime is sending addonListChanged events. The reason for this test is because the
  // previous implementation was updating the USB runtime extensions list when the _local_
  // AddonManager was updated.
  info("Remove the extension from the remote client WITHOUT sending an event");
  usbClient.listAddons = () => [];

  info("Simulate an addon update on the ThisFirefox client");
  usbMocks.thisFirefoxClient._eventEmitter.emit("addonListChanged");

  // To avoid wait for a set period of time we trigger another async update, adding a new
  // tab. We assume that if the addon update mechanism had started, it would also be done
  // when the new tab was processed.
  info("Wait until the tab target for 'http://some.random/url.com' appears");
  const testTab = { outerWindowID: 0, url: "http://some.random/url.com" };
  usbClient.listTabs = () => ({ tabs: [testTab] });
  usbClient._eventEmitter.emit("tabListChanged");
  await waitUntil(() => findDebugTargetByText("http://some.random/url.com", document));

  ok(findDebugTargetByText("Test extension name", document),
    "The test extension is still visible");

  info("Emit `addonListChanged` on usbClient and wait for the target list to update");
  usbClient._eventEmitter.emit("addonListChanged");
  await waitUntil(() => !findDebugTargetByText("Test extension name", document));

  await removeTab(tab);
});
