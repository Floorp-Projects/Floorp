/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RUNTIME_ID = "test-runtime-id";
const RUNTIME_DEVICE_NAME = "test device name";
const RUNTIME_APP_NAME = "TestApp";

const OTHER_RUNTIME_ID = "other-runtime-id";
const OTHER_RUNTIME_APP_NAME = "OtherApp";

// Test that USB runtimes are ot disconnected on refresh.
add_task(async function() {
  const mocks = new Mocks();

  const { document, tab } = await openAboutDebugging();

  info("Create a first runtime and connect to it");
  mocks.createUSBRuntime(RUNTIME_ID, {
    deviceName: RUNTIME_DEVICE_NAME,
    name: RUNTIME_APP_NAME,
  });
  mocks.emitUSBUpdate();

  await connectToRuntime(RUNTIME_DEVICE_NAME, document);
  await selectRuntime(RUNTIME_DEVICE_NAME, RUNTIME_APP_NAME, document);

  info("Create a second runtime and click on Refresh Devices");
  mocks.createUSBRuntime(OTHER_RUNTIME_ID, {
    deviceName: OTHER_RUNTIME_APP_NAME,
  });

  // adb.updateRuntimes should ultimately fire the "runtime-list-updated" event.
  mocks.adbMock.adb.updateRuntimes = () => mocks.emitUSBUpdate();
  document.querySelector(".qa-refresh-devices-button").click();

  info(`Wait until the sidebar item for ${OTHER_RUNTIME_APP_NAME} appears`);
  await waitUntil(() =>
    findSidebarItemByText(OTHER_RUNTIME_APP_NAME, document)
  );

  const sidebarItem = findSidebarItemByText(RUNTIME_DEVICE_NAME, document);
  ok(
    !sidebarItem.querySelector(".qa-connect-button"),
    "Original USB runtime is still connected"
  );

  await removeTab(tab);
});
