/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_NAME = "Firefox 123";

// Test that unknown runtimes:
// - are displayed without a connect button.
// - cannot be selected
// - display a specific text ("Waiting for runtime") instead of the runtime name
add_task(async function() {
  const mocks = new Mocks();
  const { document, tab } = await openAboutDebugging();

  info("Create a mocked unknown runtime");
  let isUnknown = true;
  mocks.createUSBRuntime("test_device_id", {
    isUnknown: () => isUnknown,
    // Here shortName would rather be Unknown Runtime from adb-runtime, but we only want
    // to check the runtime name does not appear in the UI.
    shortName: RUNTIME_NAME,
  });
  mocks.emitUSBUpdate();

  info("Wait until the USB sidebar item appears");
  await waitUntil(() => findSidebarItemByText("test device name", document));

  const usbRuntimeSidebarItem = findSidebarItemByText("test device name", document);

  const itemText = usbRuntimeSidebarItem.textContent;
  ok(itemText.includes("Waiting for runtime"), "Sidebar item shows Waiting for runtime");
  ok(!itemText.includes(RUNTIME_NAME), "Sidebar item does not show the runtime name");

  const hasConnectButton = usbRuntimeSidebarItem.querySelector(".js-connect-button");
  ok(!hasConnectButton, "Connect button is not displayed");

  const hasLink = usbRuntimeSidebarItem.querySelector(".js-sidebar-link");
  ok(!hasLink, "Unknown runtime is not selectable");

  info("Update the runtime to return false for isUnknown() and emit update event");
  isUnknown = false;
  mocks.emitUSBUpdate();

  info("Wait until connect button appears for the USB runtime");
  let updatedSidebarItem = null;
  await waitUntil(() => {
    updatedSidebarItem = findSidebarItemByText("test device name", document);
    return updatedSidebarItem && updatedSidebarItem.querySelector(".js-connect-button");
  });

  const updatedText = updatedSidebarItem.textContent;
  ok(updatedText.includes(RUNTIME_NAME), "Sidebar item shows the runtime name");

  await removeTab(tab);
});
