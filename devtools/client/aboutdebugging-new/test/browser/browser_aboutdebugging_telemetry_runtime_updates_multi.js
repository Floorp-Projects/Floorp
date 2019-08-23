/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-telemetry.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-telemetry.js",
  this
);

const DEVICE_A = "Device A";
const USB_RUNTIME_1 = {
  id: "runtime-id-1",
  deviceName: DEVICE_A,
  name: "Runtime 1",
  shortName: "R1",
};

const USB_RUNTIME_2 = {
  id: "runtime-id-2",
  deviceName: DEVICE_A,
  name: "Runtime 2",
  shortName: "R2",
};

const DEVICE_A_EXTRAS = {
  connection_type: "usb",
  device_name: DEVICE_A,
};

const RUNTIME_1_EXTRAS = {
  connection_type: "usb",
  device_name: USB_RUNTIME_1.deviceName,
  runtime_name: USB_RUNTIME_1.shortName,
};

const RUNTIME_2_EXTRAS = {
  connection_type: "usb",
  device_name: USB_RUNTIME_2.deviceName,
  runtime_name: USB_RUNTIME_2.shortName,
};

/**
 * Test runtime update events when a device is connected/disconnected with multiple
 * runtimes available on the same device.
 */
add_task(async function() {
  // enable USB devices mocks
  const mocks = new Mocks();
  setupTelemetryTest();

  const { tab, document } = await openAboutDebugging();

  const sessionId = getOpenEventSessionId();
  ok(!isNaN(sessionId), "Open event has a valid session id");

  info("Add two runtimes on the same device at the same time");
  mocks.createUSBRuntime(USB_RUNTIME_1.id, {
    deviceName: USB_RUNTIME_1.deviceName,
    name: USB_RUNTIME_1.name,
    shortName: USB_RUNTIME_1.shortName,
  });
  mocks.createUSBRuntime(USB_RUNTIME_2.id, {
    deviceName: USB_RUNTIME_2.deviceName,
    name: USB_RUNTIME_2.name,
    shortName: USB_RUNTIME_2.shortName,
  });
  mocks.emitUSBUpdate();
  await waitUntil(() =>
    findSidebarItemByText(USB_RUNTIME_1.shortName, document)
  );
  await waitUntil(() =>
    findSidebarItemByText(USB_RUNTIME_2.shortName, document)
  );

  checkTelemetryEvents(
    [
      { method: "device_added", extras: DEVICE_A_EXTRAS },
      { method: "runtime_added", extras: RUNTIME_1_EXTRAS },
      { method: "runtime_added", extras: RUNTIME_2_EXTRAS },
    ],
    sessionId
  );

  info("Remove both runtimes at once to simulate a device disconnection");
  mocks.removeRuntime(USB_RUNTIME_1.id);
  mocks.removeRuntime(USB_RUNTIME_2.id);
  mocks.emitUSBUpdate();
  await waitUntil(
    () =>
      !findSidebarItemByText(USB_RUNTIME_1.name, document) &&
      !findSidebarItemByText(USB_RUNTIME_1.shortName, document)
  );
  await waitUntil(
    () =>
      !findSidebarItemByText(USB_RUNTIME_2.name, document) &&
      !findSidebarItemByText(USB_RUNTIME_2.shortName, document)
  );

  checkTelemetryEvents(
    [
      { method: "runtime_removed", extras: RUNTIME_1_EXTRAS },
      { method: "runtime_removed", extras: RUNTIME_2_EXTRAS },
      { method: "device_removed", extras: DEVICE_A_EXTRAS },
    ],
    sessionId
  );

  await removeTab(tab);
});
