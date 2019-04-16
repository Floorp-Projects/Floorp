/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_ID = "RUNTIME_ID";
const RUNTIME_DEVICE_NAME = "RUNTIME_DEVICE_NAME";
const RUNTIME_SHORT_NAME = "testshort";

// Test that USB runtimes appear and disappear from the sidebar.
add_task(async function() {
  const mocks = new Mocks();

  const { document, tab } = await openAboutDebugging();

  mocks.createUSBRuntime(RUNTIME_ID, {
    deviceName: RUNTIME_DEVICE_NAME,
    shortName: RUNTIME_SHORT_NAME,
  });
  mocks.emitUSBUpdate();

  info("Wait until the USB sidebar item appears");
  await waitUntil(() => findSidebarItemByText(RUNTIME_DEVICE_NAME, document));
  const usbRuntimeSidebarItem = findSidebarItemByText(RUNTIME_DEVICE_NAME, document);
  ok(usbRuntimeSidebarItem.textContent.includes(RUNTIME_SHORT_NAME),
    "The short name of the usb runtime is visible");

  mocks.removeUSBRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();
  await waitUntilUsbDeviceIsUnplugged(RUNTIME_DEVICE_NAME, document);

  await removeTab(tab);
});
