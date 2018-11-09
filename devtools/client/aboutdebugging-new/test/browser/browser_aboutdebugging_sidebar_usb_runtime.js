/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from mocks/head-usb-runtimes-mock.js */

"use strict";

// Load USB Runtimes mock module
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "mocks/head-usb-runtimes-mock.js", this);

// Test that USB runtimes appear and disappear from the sidebar.
add_task(async function() {
  const usbRuntimesMock = createUsbRuntimesMock();
  const observerMock = addObserverMock(usbRuntimesMock);
  enableUsbRuntimesMock(usbRuntimesMock);

  // Disable our mock when the test ends.
  registerCleanupFunction(() => {
    disableUsbRuntimesMock();
  });

  const { document, tab } = await openAboutDebugging();

  usbRuntimesMock.getUSBRuntimes = function() {
    return [{
      id: "test_device_id",
      _socketPath: "test/path",
      deviceName: "test device name",
      shortName: "testshort",
    }];
  };
  observerMock.emit("runtime-list-updated");

  info("Wait until the USB sidebar item appears");
  await waitUntil(() => findSidebarItemByText("test device name", document));
  const usbRuntimeSidebarItem = findSidebarItemByText("test device name", document);
  ok(usbRuntimeSidebarItem.textContent.includes("testshort"),
    "The short name of the usb runtime is visible");

  usbRuntimesMock.getUSBRuntimes = function() {
    return [];
  };
  observerMock.emit("runtime-list-updated");

  info("Wait until the USB sidebar item disappears");
  await waitUntil(() => !findSidebarItemByText("test device name", document));

  await removeTab(tab);
});
