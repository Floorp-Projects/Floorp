/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_NAME = "Firefox 123";
const DEVICE_NAME = "DEVICE_NAME";
const DEVICE_ID = "DEVICE_ID";
const RUNTIME_ID = "RUNTIME_ID";

// Test that unknown runtimes:
// - are displayed without a connect button.
// - cannot be selected
// - display a specific text ("Waiting for runtime") instead of the runtime name
add_task(async function() {
  const mocks = new Mocks();
  const { document, tab } = await openAboutDebugging();

  info("Create a device without a corresponding runtime");
  mocks.addDevice(DEVICE_ID, DEVICE_NAME);
  mocks.emitUSBUpdate();

  info("Wait until the USB sidebar item appears");
  await waitUntil(() => findSidebarItemByText(DEVICE_NAME, document));

  const usbRuntimeSidebarItem = findSidebarItemByText(DEVICE_NAME, document);

  const itemText = usbRuntimeSidebarItem.textContent;
  ok(itemText.includes("Waiting for browser"), "Sidebar item shows Waiting for browser");
  ok(!itemText.includes(RUNTIME_NAME), "Sidebar item does not show the runtime name");

  const hasConnectButton = usbRuntimeSidebarItem.querySelector(".js-connect-button");
  ok(!hasConnectButton, "Connect button is not displayed");

  const hasLink = usbRuntimeSidebarItem.querySelector(".js-sidebar-link");
  ok(!hasLink, "Unknown runtime is not selectable");

  info("Add a valid runtime for the same device id and emit update event");
  mocks.createUSBRuntime(RUNTIME_ID, {
    deviceId: DEVICE_ID,
    deviceName: DEVICE_NAME,
    shortName: RUNTIME_NAME,
  });
  mocks.removeDevice(DEVICE_ID);
  mocks.emitUSBUpdate();

  info("Wait until connect button appears for the USB runtime");
  let updatedSidebarItem = null;
  await waitUntil(() => {
    updatedSidebarItem = findSidebarItemByText(DEVICE_NAME, document);
    return updatedSidebarItem && updatedSidebarItem.querySelector(".js-connect-button");
  });

  const updatedText = updatedSidebarItem.textContent;
  ok(updatedText.includes(RUNTIME_NAME), "Sidebar item shows the runtime name");

  await removeTab(tab);
});
