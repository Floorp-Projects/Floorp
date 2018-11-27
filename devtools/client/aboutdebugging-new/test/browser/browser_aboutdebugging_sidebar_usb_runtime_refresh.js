/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_ID = "test-runtime-id";
const RUNTIME_DEVICE_NAME = "test device name";
const RUNTIME_APP_NAME = "TestApp";

const OTHER_RUNTIME_ID = "other-runtime-id";
const OTHER_RUNTIME_APP_NAME = "OtherApp";

/* import-globals-from mocks/head-usb-mocks.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "mocks/head-usb-mocks.js", this);

// Test that USB runtimes are ot disconnected on refresh.
add_task(async function() {
  const usbMocks = new UsbMocks();
  usbMocks.enableMocks();
  registerCleanupFunction(() => usbMocks.disableMocks());

  const { document, tab } = await openAboutDebugging();

  info("Create a first runtime and connect to it");
  usbMocks.createRuntime(RUNTIME_ID, {
    deviceName: RUNTIME_DEVICE_NAME,
    name: RUNTIME_APP_NAME,
  });
  usbMocks.emitUpdate();

  await connectToRuntime(RUNTIME_DEVICE_NAME, document);
  await selectRuntime(RUNTIME_DEVICE_NAME, RUNTIME_APP_NAME, document);

  info("Create a second runtime and click on Refresh Devices");
  usbMocks.createRuntime(OTHER_RUNTIME_ID, {
    deviceName: OTHER_RUNTIME_APP_NAME,
  });

  // Mock the refreshUSBRuntimes to emit an update.
  usbMocks.usbRuntimesMock.refreshUSBRuntimes = () => usbMocks.emitUpdate();
  document.querySelector(".js-refresh-devices-button").click();

  info(`Wait until the sidebar item for ${OTHER_RUNTIME_APP_NAME} appears`);
  await waitUntil(() => findSidebarItemByText(OTHER_RUNTIME_APP_NAME, document));

  const sidebarItem = findSidebarItemByText(RUNTIME_DEVICE_NAME, document);
  ok(!sidebarItem.querySelector(".js-connect-button"),
    "Original USB runtime is still connected");

  await removeTab(tab);
});
