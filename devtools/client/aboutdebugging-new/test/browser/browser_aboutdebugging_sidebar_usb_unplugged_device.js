/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_NAME = "RUNTIME_NAME_1";
const DEVICE_NAME = "DEVICE_NAME_1";
const DEVICE_ID = "DEVICE_ID_1";
const RUNTIME_ID = "RUNTIME_ID_1";

const RUNTIME_NAME_2 = "RUNTIME_NAME_2";
const DEVICE_NAME_2 = "DEVICE_NAME_2";
const DEVICE_ID_2 = "DEVICE_ID_2";
const RUNTIME_ID_2 = "RUNTIME_ID_2";

// Test that removed USB devices are still visible as "Unplugged devices", until
// about:debugging is reloaded.
add_task(async function() {
  const mocks = new Mocks();
  let { document, tab } = await openAboutDebugging();

  info("Create a mocked USB runtime");
  mocks.createUSBRuntime(RUNTIME_ID, {
    deviceId: DEVICE_ID,
    deviceName: DEVICE_NAME,
    shortName: RUNTIME_NAME,
  });
  mocks.emitUSBUpdate();

  info("Wait until the USB sidebar item appears");
  await waitUntil(() => findSidebarItemByText(DEVICE_NAME, document));
  const sidebarItem = findSidebarItemByText(DEVICE_NAME, document);
  ok(
    sidebarItem.textContent.includes(RUNTIME_NAME),
    "Sidebar item shows the runtime name"
  );

  mocks.removeUSBRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();
  await waitUntilUsbDeviceIsUnplugged(DEVICE_NAME, document);

  const unpluggedItem = findSidebarItemByText(DEVICE_NAME, document);
  ok(
    unpluggedItem.querySelector(".qa-runtime-item-unplugged"),
    "Sidebar item is shown as `Unplugged…`"
  );

  info("Reload about:debugging");
  document = await reloadAboutDebugging(tab);

  info(
    "Add another mocked USB runtime, to make sure the sidebar items are rendered."
  );
  mocks.createUSBRuntime(RUNTIME_ID_2, {
    deviceId: DEVICE_ID_2,
    deviceName: DEVICE_NAME_2,
    shortName: RUNTIME_NAME_2,
  });
  mocks.emitUSBUpdate();

  info("Wait until the other USB sidebar item appears");
  await waitUntil(() => findSidebarItemByText(DEVICE_NAME_2, document));
  ok(
    !findSidebarItemByText(DEVICE_NAME, document),
    "Unplugged device is no longer displayed after reloading aboutdebugging"
  );

  await removeTab(tab);
});
