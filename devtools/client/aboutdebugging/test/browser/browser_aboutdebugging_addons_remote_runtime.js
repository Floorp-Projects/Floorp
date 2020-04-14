/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const NETWORK_RUNTIME_HOST = "localhost:6080";
const NETWORK_RUNTIME_APP_NAME = "TestNetworkApp";
const USB_RUNTIME_ID = "test-runtime-id";
const USB_RUNTIME_DEVICE_NAME = "test device name";
const USB_RUNTIME_APP_NAME = "TestUsbApp";

// Test that addons are displayed and updated for USB runtimes when expected.
add_task(async function() {
  const mocks = new Mocks();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  info("Prepare USB client mock");
  const usbClient = mocks.createUSBRuntime(USB_RUNTIME_ID, {
    deviceName: USB_RUNTIME_DEVICE_NAME,
    name: USB_RUNTIME_APP_NAME,
  });
  mocks.emitUSBUpdate();

  info("Test addons in runtime page for USB client");
  await connectToRuntime(USB_RUNTIME_DEVICE_NAME, document);
  await selectRuntime(USB_RUNTIME_DEVICE_NAME, USB_RUNTIME_APP_NAME, document);
  await testAddonsOnMockedRemoteClient(
    usbClient,
    mocks.thisFirefoxClient,
    document
  );

  info("Prepare Network client mock");
  const networkClient = mocks.createNetworkRuntime(NETWORK_RUNTIME_HOST, {
    name: NETWORK_RUNTIME_APP_NAME,
  });

  info("Test addons in runtime page for Network client");
  await connectToRuntime(NETWORK_RUNTIME_HOST, document);
  await selectRuntime(NETWORK_RUNTIME_HOST, NETWORK_RUNTIME_APP_NAME, document);
  await testAddonsOnMockedRemoteClient(
    networkClient,
    mocks.thisFirefoxClient,
    document
  );

  await removeTab(tab);
});

/**
 * Check that addons are visible in the runtime page for a remote client (USB or network).
 */
async function testAddonsOnMockedRemoteClient(
  remoteClient,
  firefoxClient,
  document
) {
  const extensionPane = getDebugTargetPane("Extensions", document);
  info("Check an empty target pane message is displayed");
  ok(
    extensionPane.querySelector(".qa-debug-target-list-empty"),
    "Extensions list is empty"
  );

  info("Add an extension to the remote client");
  const addon = { name: "Test extension name", debuggable: true };
  const temporaryAddon = {
    name: "Test temporary extension",
    debuggable: true,
    temporarilyInstalled: true,
  };
  remoteClient.listAddons = () => [addon, temporaryAddon];
  remoteClient._eventEmitter.emit("addonListChanged");

  info("Wait until the extension appears");
  await waitUntil(
    () => !extensionPane.querySelector(".qa-debug-target-list-empty")
  );

  const extensionTarget = findDebugTargetByText(
    "Test extension name",
    document
  );
  ok(extensionTarget, "Extension target appeared for the remote runtime");

  const temporaryExtensionTarget = findDebugTargetByText(
    "Test temporary extension",
    document
  );
  ok(
    temporaryExtensionTarget,
    "Temporary Extension target appeared for the remote runtime"
  );

  const removeButton = temporaryExtensionTarget.querySelector(
    ".qa-temporary-extension-remove-button"
  );
  const reloadButton = temporaryExtensionTarget.querySelector(
    ".qa-temporary-extension-reload-button"
  );
  ok(!removeButton, "No remove button expected for the temporary extension");
  ok(!reloadButton, "No reload button expected for the temporary extension");

  // The goal here is to check that runtimes addons are only updated when the remote
  // runtime is sending addonListChanged events. The reason for this test is because the
  // previous implementation was updating the remote runtime extensions list when the
  // _local_ AddonManager was updated.
  info("Remove the extension from the remote client WITHOUT sending an event");
  remoteClient.listAddons = () => [];

  info("Simulate an addon update on the ThisFirefox client");
  firefoxClient._eventEmitter.emit("addonListChanged");

  // To avoid wait for a set period of time we trigger another async update, adding a new
  // tab. We assume that if the addon update mechanism had started, it would also be done
  // when the new tab was processed.
  info("Wait until the tab target for 'http://some.random/url.com' appears");
  const testTab = {
    retrieveAsyncFormData: () => {},
    outerWindowID: 0,
    traits: {
      getFavicon: true,
      hasTabInfo: true,
    },
    url: "http://some.random/url.com",
  };
  remoteClient.listTabs = () => [testTab];
  remoteClient._eventEmitter.emit("tabListChanged");
  await waitUntil(() =>
    findDebugTargetByText("http://some.random/url.com", document)
  );

  ok(
    findDebugTargetByText("Test extension name", document),
    "The test extension is still visible"
  );

  info(
    "Emit `addonListChanged` on remoteClient and wait for the target list to update"
  );
  remoteClient._eventEmitter.emit("addonListChanged");
  await waitUntil(
    () => !findDebugTargetByText("Test extension name", document)
  );
}
