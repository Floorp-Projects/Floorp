/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-telemetry.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-telemetry.js", this);

const USB_RUNTIME = {
  id: "runtime-id-1",
  deviceName: "Device A",
  name: "Runtime 1",
  shortName: "R1",
};

/**
 * Check that telemetry events for connection attempts are correctly recorded in various
 * scenarios:
 * - successful connection
 * - successful connection after showing the timeout warning
 * - failed connection
 * - connection timeout
 */
add_task(async function testSuccessfulConnectionAttempt() {
  const { doc, mocks, runtimeId, sessionId, tab } = await setupConnectionAttemptTest();

  await connectToRuntime(USB_RUNTIME.deviceName, doc);

  const connectionEvents = checkTelemetryEvents([
    { method: "runtime_connected", extras: { runtime_id: runtimeId } },
    { method: "connection_attempt", extras: getEventExtras("start", runtimeId) },
    { method: "connection_attempt", extras: getEventExtras("success", runtimeId) },
  ], sessionId).filter(({method}) => method === "connection_attempt");

  checkConnectionId(connectionEvents);

  await removeUsbRuntime(USB_RUNTIME, mocks, doc);
  await removeTab(tab);
});

add_task(async function testFailedConnectionAttempt() {
  const { doc, mocks, runtimeId, sessionId, tab } = await setupConnectionAttemptTest();
  mocks.runtimeClientFactoryMock.createClientForRuntime = async (runtime) => {
    throw new Error("failed");
  };

  info("Try to connect to the runtime and wait for the connection error message");
  const usbRuntimeSidebarItem = findSidebarItemByText(USB_RUNTIME.deviceName, doc);
  const connectButton = usbRuntimeSidebarItem.querySelector(".qa-connect-button");
  connectButton.click();
  await waitUntil(() => usbRuntimeSidebarItem.querySelector(".qa-connection-error"));

  const connectionEvents = checkTelemetryEvents([
    { method: "connection_attempt", extras: getEventExtras("start", runtimeId) },
    { method: "connection_attempt", extras: getEventExtras("failed", runtimeId) },
  ], sessionId).filter(({method}) => method === "connection_attempt");

  checkConnectionId(connectionEvents);

  await removeUsbRuntime(USB_RUNTIME, mocks, doc);
  await removeTab(tab);
});

add_task(async function testPendingConnectionAttempt() {
  info("Set timeout preferences to avoid cancelling the connection");
  await pushPref("devtools.aboutdebugging.test-connection-timing-out-delay", 100);
  await pushPref("devtools.aboutdebugging.test-connection-cancel-delay", 100000);

  const { doc, mocks, runtimeId, sessionId, tab } = await setupConnectionAttemptTest();

  info("Simulate a pending connection");
  let resumeConnection;
  const resumeConnectionPromise = new Promise(r => {
    resumeConnection = r;
  });
  mocks.runtimeClientFactoryMock.createClientForRuntime = async (runtime) => {
    await resumeConnectionPromise;
    return mocks._clients[runtime.type][runtime.id];
  };

  info("Click on the connect button and wait for the warning message");
  const usbRuntimeSidebarItem = findSidebarItemByText(USB_RUNTIME.deviceName, doc);
  const connectButton = usbRuntimeSidebarItem.querySelector(".qa-connect-button");
  connectButton.click();
  await waitUntil(() => doc.querySelector(".qa-connection-not-responding"));

  info("Resume the connection and wait for the connection to succeed");
  resumeConnection();
  await waitUntil(() => !usbRuntimeSidebarItem.querySelector(".qa-connect-button"));

  const connectionEvents = checkTelemetryEvents([
    { method: "runtime_connected", extras: { runtime_id: runtimeId } },
    { method: "connection_attempt", extras: getEventExtras("start", runtimeId) },
    { method: "connection_attempt", extras: getEventExtras("not responding", runtimeId) },
    { method: "connection_attempt", extras: getEventExtras("success", runtimeId) },
  ], sessionId).filter(({method}) => method === "connection_attempt");
  checkConnectionId(connectionEvents);

  await removeUsbRuntime(USB_RUNTIME, mocks, doc);
  await removeTab(tab);
});

add_task(async function testCancelledConnectionAttempt() {
  info("Set timeout preferences to quickly cancel the connection");
  await pushPref("devtools.aboutdebugging.test-connection-timing-out-delay", 100);
  await pushPref("devtools.aboutdebugging.test-connection-cancel-delay", 1000);

  const { doc, mocks, runtimeId, sessionId, tab } = await setupConnectionAttemptTest();

  info("Simulate a connection timeout");
  mocks.runtimeClientFactoryMock.createClientForRuntime = async (runtime) => {
    await new Promise(r => {});
  };

  info("Click on the connect button and wait for the error message");
  const usbRuntimeSidebarItem = findSidebarItemByText(USB_RUNTIME.deviceName, doc);
  const connectButton = usbRuntimeSidebarItem.querySelector(".qa-connect-button");
  connectButton.click();
  await waitUntil(() => usbRuntimeSidebarItem.querySelector(".qa-connection-timeout"));

  const connectionEvents = checkTelemetryEvents([
    { method: "connection_attempt", extras: getEventExtras("start", runtimeId) },
    { method: "connection_attempt", extras: getEventExtras("not responding", runtimeId) },
    { method: "connection_attempt", extras: getEventExtras("cancelled", runtimeId) },
  ], sessionId).filter(({method}) => method === "connection_attempt");
  checkConnectionId(connectionEvents);

  await removeUsbRuntime(USB_RUNTIME, mocks, doc);
  await removeTab(tab);
});

function checkConnectionId(connectionEvents) {
  const connectionId = connectionEvents[0].extras.connection_id;
  ok(!!connectionId, "Found a valid connection id in the first connection_attempt event");
  for (const evt of connectionEvents) {
    is(evt.extras.connection_id, connectionId,
      "All connection_attempt events share the same connection id");
  }
}

// Small helper to create the expected event extras object for connection_attempt events
function getEventExtras(status, runtimeId) {
  return {
    connection_type: "usb",
    runtime_id: runtimeId,
    status,
  };
}

// Open about:debugging, setup telemetry, mocks and create a mocked USB runtime.
async function setupConnectionAttemptTest() {
  const mocks = new Mocks();
  setupTelemetryTest();

  const { tab, document } = await openAboutDebugging();

  const sessionId = getOpenEventSessionId();
  ok(!isNaN(sessionId), "Open event has a valid session id");

  mocks.createUSBRuntime(USB_RUNTIME.id, {
    deviceName: USB_RUNTIME.deviceName,
    name: USB_RUNTIME.name,
    shortName: USB_RUNTIME.shortName,
  });
  mocks.emitUSBUpdate();

  info("Wait for the runtime to appear in the sidebar");
  await waitUntil(() => findSidebarItemByText(USB_RUNTIME.shortName, document));
  const evts = checkTelemetryEvents([
    { method: "device_added", extras: {} },
    { method: "runtime_added", extras: {} },
  ], sessionId);

  const runtimeId = evts.filter(e => e.method === "runtime_added")[0].extras.runtime_id;
  return { doc: document, mocks, runtimeId, sessionId, tab };
}

async function removeUsbRuntime(runtime, mocks, doc) {
  mocks.removeRuntime(runtime.id);
  mocks.emitUSBUpdate();
  await waitUntil(() => !findSidebarItemByText(runtime.name, doc) &&
                        !findSidebarItemByText(runtime.shortName, doc));
}
