/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_ID = "test-runtime-id";
const RUNTIME_DEVICE_NAME = "test device name";

// Test that USB runtimes appear and disappear from the sidebar,
// as well as their connect button.
add_task(async function() {
  const mocks = new Mocks();

  const { document, tab } = await openAboutDebugging();

  mocks.createUSBRuntime(RUNTIME_ID, { deviceName: RUNTIME_DEVICE_NAME });
  mocks.emitUSBUpdate();

  info("Wait until the USB sidebar item appears");
  await waitUntil(() => findSidebarItemByText(RUNTIME_DEVICE_NAME, document));
  const usbRuntimeSidebarItem = findSidebarItemByText(RUNTIME_DEVICE_NAME, document);
  const connectButton = usbRuntimeSidebarItem.querySelector(".js-connect-button");
  ok(connectButton, "Connect button is displayed for the USB runtime");

  info("Click on the connect button and wait until it disappears");
  connectButton.click();
  await waitUntil(() => !usbRuntimeSidebarItem.querySelector(".js-connect-button"));

  info("Remove all USB runtimes");
  mocks.removeUSBRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();

  info("Wait until the USB sidebar item disappears");
  await waitUntil(() => !findSidebarItemByText(RUNTIME_DEVICE_NAME, document));

  await removeTab(tab);
});
