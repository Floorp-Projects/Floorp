/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-telemetry.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-telemetry.js", this);

const NETWORK_RUNTIME = {
  host: "localhost:1234",
  // No device name for network runtimes.
  name: "Local Network Runtime",
};

/**
 * Test runtime update events for network runtimes.
 */
add_task(async function testNetworkRuntimeUpdates() {
  // enable USB devices mocks
  const mocks = new Mocks();
  setupTelemetryTest();

  const { tab, document } = await openAboutDebugging();

  const sessionId = getOpenEventSessionId();
  ok(!isNaN(sessionId), "Open event has a valid session id");

  info("Add a network runtime");
  await addNetworkRuntime(NETWORK_RUNTIME, mocks, document);

  // Before the connection, we don't have any information about the runtime.
  // Device information is also not available to network runtimes.
  const networkRuntimeExtras = {
    "connection_type": "network",
    "device_name": "",
    "runtime_name": "",
  };

  // Once connected we should be able to log a valid runtime name.
  const connectedNetworkRuntimeExtras = Object.assign({}, networkRuntimeExtras, {
    "runtime_name": NETWORK_RUNTIME.name,
  });

  // For network runtimes, we don't have any device information, so we shouldn't have any
  // device_added event.
  checkTelemetryEvents([
    { method: "runtime_added", extras: networkRuntimeExtras },
  ], sessionId);

  await connectToRuntime(NETWORK_RUNTIME.host, document);
  checkTelemetryEvents([
    { method: "runtime_connected", extras: connectedNetworkRuntimeExtras },
    { method: "connection_attempt", extras: { status: "start" } },
    { method: "connection_attempt", extras: { status: "success" } },
  ], sessionId);

  info("Remove network runtime");
  mocks.removeRuntime(NETWORK_RUNTIME.host);
  await waitUntil(() => !findSidebarItemByText(NETWORK_RUNTIME.host, document));
  // Similarly we should not have any device removed event.
  checkTelemetryEvents([
    { method: "runtime_disconnected", extras: connectedNetworkRuntimeExtras },
    { method: "runtime_removed", extras: networkRuntimeExtras },
  ], sessionId);

  await removeTab(tab);
});

async function addNetworkRuntime(runtime, mocks, doc) {
  mocks.createNetworkRuntime(runtime.host, {
    name: runtime.name,
  });

  info("Wait for the Network Runtime to appear in the sidebar");
  await waitUntil(() => findSidebarItemByText(runtime.host, doc));
}
