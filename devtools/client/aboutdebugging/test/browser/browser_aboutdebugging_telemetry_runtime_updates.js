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
 * Check that telemetry events are recorded for USB runtimes when:
 * - adding a device/runtime
 * - removing a device/runtime
 * - connecting to a runtime
 */
add_task(async function testUsbRuntimeUpdates() {
  // enable USB devices mocks
  const mocks = new Mocks();
  setupTelemetryTest();

  const { tab, document } = await openAboutDebugging();

  const sessionId = getOpenEventSessionId();
  ok(!isNaN(sessionId), "Open event has a valid session id");

  await addUsbRuntime(USB_RUNTIME_1, mocks, document);

  let evts = checkTelemetryEvents(
    [
      { method: "device_added", extras: DEVICE_A_EXTRAS },
      { method: "runtime_added", extras: RUNTIME_1_EXTRAS },
    ],
    sessionId
  );

  // Now that a first telemetry event has been logged for RUNTIME_1, retrieve the id
  // generated for telemetry, and check that we keep logging the same id for all events
  // related to runtime 1.
  const runtime1Id = evts.filter(e => e.method === "runtime_added")[0].extras
    .runtime_id;
  const runtime1Extras = Object.assign({}, RUNTIME_1_EXTRAS, {
    runtime_id: runtime1Id,
  });
  // Same as runtime1Extras, but the runtime name should be the complete one.
  const runtime1ConnectedExtras = Object.assign({}, runtime1Extras, {
    runtime_name: USB_RUNTIME_1.name,
  });

  await connectToRuntime(USB_RUNTIME_1.deviceName, document);

  checkTelemetryEvents(
    [
      { method: "runtime_connected", extras: runtime1ConnectedExtras },
      { method: "connection_attempt", extras: { status: "start" } },
      { method: "connection_attempt", extras: { status: "success" } },
    ],
    sessionId
  );

  info("Add a second runtime");
  await addUsbRuntime(USB_RUNTIME_2, mocks, document);
  evts = checkTelemetryEvents(
    [{ method: "runtime_added", extras: RUNTIME_2_EXTRAS }],
    sessionId
  );

  // Similar to what we did for RUNTIME_1,w e want to check we reuse the same telemetry id
  // for all the events related to RUNTIME_2.
  const runtime2Id = evts.filter(e => e.method === "runtime_added")[0].extras
    .runtime_id;
  const runtime2Extras = Object.assign({}, RUNTIME_2_EXTRAS, {
    runtime_id: runtime2Id,
  });

  info("Remove runtime 1");
  await removeUsbRuntime(USB_RUNTIME_1, mocks, document);

  checkTelemetryEvents(
    [
      { method: "runtime_disconnected", extras: runtime1ConnectedExtras },
      { method: "runtime_removed", extras: runtime1Extras },
    ],
    sessionId
  );

  info("Remove runtime 2");
  await removeUsbRuntime(USB_RUNTIME_2, mocks, document);

  checkTelemetryEvents(
    [
      { method: "runtime_removed", extras: runtime2Extras },
      { method: "device_removed", extras: DEVICE_A_EXTRAS },
    ],
    sessionId
  );

  await removeTab(tab);
});

async function addUsbRuntime(runtime, mocks, doc) {
  mocks.createUSBRuntime(runtime.id, {
    deviceName: runtime.deviceName,
    name: runtime.name,
    shortName: runtime.shortName,
  });
  mocks.emitUSBUpdate();

  info("Wait for the runtime to appear in the sidebar");
  await waitUntil(() => findSidebarItemByText(runtime.shortName, doc));
}

async function removeUsbRuntime(runtime, mocks, doc) {
  mocks.removeRuntime(runtime.id);
  mocks.emitUSBUpdate();
  await waitUntil(
    () =>
      !findSidebarItemByText(runtime.name, doc) &&
      !findSidebarItemByText(runtime.shortName, doc)
  );
}
