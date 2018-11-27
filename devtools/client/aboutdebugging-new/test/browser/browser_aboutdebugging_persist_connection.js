/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_ID = "test-runtime-id";
const RUNTIME_DEVICE_NAME = "test device name";
const RUNTIME_APP_NAME = "TestApp";

/* import-globals-from mocks/head-usb-mocks.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "mocks/head-usb-mocks.js", this);

// Test that remote runtime connections are persisted across about:debugging reloads.
add_task(async function() {
  const usbMocks = new UsbMocks();
  usbMocks.enableMocks();
  registerCleanupFunction(() => usbMocks.disableMocks());

  let { document, tab } = await openAboutDebugging();

  const usbClient = usbMocks.createRuntime(RUNTIME_ID, {
    name: RUNTIME_APP_NAME,
    deviceName: RUNTIME_DEVICE_NAME,
  });
  usbMocks.emitUpdate();

  await connectToRuntime(RUNTIME_DEVICE_NAME, document);
  await selectRuntime(RUNTIME_DEVICE_NAME, RUNTIME_APP_NAME, document);

  info("Reload about:debugging");
  document = await reloadAboutDebugging(tab);
  usbMocks.emitUpdate();

  info("Wait until the remote runtime appears as connected");
  await waitUntil(() => {
    const sidebarItem = findSidebarItemByText(RUNTIME_DEVICE_NAME, document);
    return sidebarItem && !sidebarItem.querySelector(".js-connect-button");
  });

  // Remove the runtime without emitting an update.
  // This is what happens today when we simply close Firefox for Android.
  info("Remove the runtime from the list of USB runtimes");
  usbMocks.removeRuntime(RUNTIME_ID);

  info("Emit 'closed' on the client and wait for the sidebar item to disappear");
  usbClient._eventEmitter.emit("closed");
  await waitUntil(() => !findSidebarItemByText(RUNTIME_DEVICE_NAME, document));

  info("Remove the tab");
  await removeTab(tab);
});
