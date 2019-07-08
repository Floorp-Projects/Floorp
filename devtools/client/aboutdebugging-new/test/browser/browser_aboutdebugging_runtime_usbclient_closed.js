/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const NETWORK_RUNTIME_HOST = "localhost:6080";
const NETWORK_RUNTIME_APP_NAME = "TestNetworkApp";
const USB_RUNTIME_ID = "test-runtime-id";
const USB_DEVICE_NAME = "test device name";
const USB_APP_NAME = "TestApp";

// Test that about:debugging navigates back to the default page when a USB device is
// unplugged.
add_task(async function testUsbDeviceUnplugged() {
  const mocks = new Mocks();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  mocks.createUSBRuntime(USB_RUNTIME_ID, {
    deviceName: USB_DEVICE_NAME,
    name: USB_APP_NAME,
  });
  mocks.emitUSBUpdate();

  info("Connect to and select the USB device");
  await connectToRuntime(USB_DEVICE_NAME, document);
  await selectRuntime(USB_DEVICE_NAME, USB_APP_NAME, document);

  info("Simulate a device unplugged");
  mocks.removeUSBRuntime(USB_RUNTIME_ID);
  mocks.emitUSBUpdate();
  await waitUntilUsbDeviceIsUnplugged(USB_DEVICE_NAME, document);

  is(
    document.location.hash,
    `#/runtime/this-firefox`,
    "Redirection to the default page (this-firefox)"
  );

  await removeTab(tab);
});

// Test that about:debugging navigates back to the default page when the server for the
// current USB runtime is closed.
add_task(async function testUsbClientDisconnected() {
  const mocks = new Mocks();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const usbClient = mocks.createUSBRuntime(USB_RUNTIME_ID, {
    deviceName: USB_DEVICE_NAME,
    name: USB_APP_NAME,
  });
  mocks.emitUSBUpdate();

  info("Connect to and select the USB device");
  await connectToRuntime(USB_DEVICE_NAME, document);
  await selectRuntime(USB_DEVICE_NAME, USB_APP_NAME, document);

  info("Simulate a client disconnection");
  usbClient.isClosed = () => true;
  usbClient._eventEmitter.emit("closed");

  info("Wait until the connect button for this runtime appears");
  await waitUntil(() => {
    const item = findSidebarItemByText(USB_DEVICE_NAME, document);
    return item && item.querySelector(".qa-connect-button");
  });

  is(
    document.location.hash,
    `#/runtime/this-firefox`,
    "Redirection to the default page (this-firefox)"
  );
  await removeTab(tab);
});

// Test that about:debugging navigates back to the default page when the server for the
// current network runtime is closed.
add_task(async function testNetworkClientDisconnected() {
  const mocks = new Mocks();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const networkClient = mocks.createNetworkRuntime(NETWORK_RUNTIME_HOST, {
    name: NETWORK_RUNTIME_APP_NAME,
  });

  info("Connect to and select the network runtime");
  await connectToRuntime(NETWORK_RUNTIME_HOST, document);
  await selectRuntime(NETWORK_RUNTIME_HOST, NETWORK_RUNTIME_APP_NAME, document);

  info("Simulate a client disconnection");
  networkClient.isClosed = () => true;
  networkClient._eventEmitter.emit("closed");

  info("Wait until the connect button for this runtime appears");
  await waitUntil(() => {
    const item = findSidebarItemByText(NETWORK_RUNTIME_HOST, document);
    return item && item.querySelector(".qa-connect-button");
  });

  is(
    document.location.hash,
    `#/runtime/this-firefox`,
    "Redirection to the default page (this-firefox)"
  );
  await removeTab(tab);
});
