/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const NETWORK_RUNTIME_HOST = "localhost:6080";
const NETWORK_RUNTIME_APP_NAME = "TestNetworkApp";
const USB_RUNTIME_ID = "test-runtime-id";
const USB_DEVICE_NAME = "test device name";
const USB_APP_NAME = "TestApp";

// Test that remote runtime connections are persisted across about:debugging reloads.
add_task(async function() {
  const mocks = new Mocks();

  info("Test with a USB runtime");
  const usbClient = mocks.createUSBRuntime(USB_RUNTIME_ID, {
    name: USB_APP_NAME,
    deviceName: USB_DEVICE_NAME,
  });

  await testRemoteClientPersistConnection(mocks, {
    client: usbClient,
    id: USB_RUNTIME_ID,
    runtimeName: USB_APP_NAME,
    sidebarName: USB_DEVICE_NAME,
    type: "usb",
  });

  info("Test with a network runtime");
  const networkClient = mocks.createNetworkRuntime(NETWORK_RUNTIME_HOST, {
    name: NETWORK_RUNTIME_APP_NAME,
  });

  await testRemoteClientPersistConnection(mocks, {
    client: networkClient,
    id: NETWORK_RUNTIME_HOST,
    runtimeName: NETWORK_RUNTIME_APP_NAME,
    sidebarName: NETWORK_RUNTIME_HOST,
    type: "network",
  });
});

async function testRemoteClientPersistConnection(
  mocks,
  { client, id, runtimeName, sidebarName, type }
) {
  info("Open about:debugging and connect to the test runtime");
  let { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await connectToRuntime(sidebarName, document);
  await selectRuntime(sidebarName, runtimeName, document);

  info("Reload about:debugging");
  document = await reloadAboutDebugging(tab);

  info("Wait until the remote runtime appears as connected");
  await waitUntil(() => {
    const sidebarItem = findSidebarItemByText(sidebarName, document);
    return sidebarItem && !sidebarItem.querySelector(".qa-connect-button");
  });

  info("Wait until the remote runtime page is selected");
  await waitUntil(() => {
    const runtimeInfo = document.querySelector(".qa-runtime-name");
    return runtimeInfo && runtimeInfo.textContent.includes(runtimeName);
  });

  // Remove the runtime without emitting an update.
  // This is what happens today when we simply close Firefox for Android.
  info("Remove the runtime from the list of remote runtimes");
  mocks.removeRuntime(id);

  info(
    "Emit 'closed' on the client and wait for the sidebar item to disappear"
  );
  client._eventEmitter.emit("closed");
  if (type === "usb") {
    await waitUntilUsbDeviceIsUnplugged(sidebarName, document);
  } else {
    await waitUntil(
      () =>
        !findSidebarItemByText(sidebarName, document) &&
        !findSidebarItemByText(runtimeName, document)
    );
  }

  info("Remove the tab");
  await removeTab(tab);
}
