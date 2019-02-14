/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that USB runtimes appear and disappear from the sidebar.
add_task(async function() {
  const mocks = new Mocks();

  const { document, tab } = await openAboutDebugging();

  mocks.createUSBRuntime("test_device_id", {
    deviceName: "test device name",
    shortName: "testshort",
  });
  mocks.emitUSBUpdate();

  info("Wait until the USB sidebar item appears");
  await waitUntil(() => findSidebarItemByText("test device name", document));
  const usbRuntimeSidebarItem = findSidebarItemByText("test device name", document);
  ok(usbRuntimeSidebarItem.textContent.includes("testshort"),
    "The short name of the usb runtime is visible");

  mocks.removeUSBRuntime("test_device_id");
  mocks.emitUSBUpdate();

  info("Wait until the USB sidebar item disappears");
  await waitUntil(() => !findSidebarItemByText("test device name", document));

  await removeTab(tab);
});
