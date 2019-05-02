/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-telemetry.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-telemetry.js", this);

const RUNTIME_ID = "test-runtime-id";
const RUNTIME_NAME = "Test Runtime";
const RUNTIME_DEVICE_NAME = "Test Device";

/**
 * Test that runtime specific actions are logged as telemetry events with the expected
 * runtime id and action type.
 */
add_task(async function testUsbRuntimeUpdates() {
  // enable USB devices mocks
  const mocks = new Mocks();
  setupTelemetryTest();

  const { tab, document } = await openAboutDebugging();

  const sessionId = getOpenEventSessionId();
  ok(!isNaN(sessionId), "Open event has a valid session id");

  const usbClient = mocks.createUSBRuntime(RUNTIME_ID, {
    deviceName: RUNTIME_DEVICE_NAME,
    name: RUNTIME_NAME,
    shortName: RUNTIME_NAME,
  });
  mocks.emitUSBUpdate();

  info("Wait for the runtime to appear in the sidebar");
  await waitUntil(() => findSidebarItemByText(RUNTIME_NAME, document));
  await connectToRuntime(RUNTIME_DEVICE_NAME, document);
  await selectRuntime(RUNTIME_DEVICE_NAME, RUNTIME_NAME, document);

  info("Read telemetry events to flush unrelated events");
  const evts = readAboutDebuggingEvents();
  const runtimeAddedEvent = evts.filter(e => e.method === "runtime_added")[0];
  const telemetryRuntimeId = runtimeAddedEvent.extras.runtime_id;

  info("Click on the toggle button and wait until the text is updated");
  const promptButton = document.querySelector(".qa-connection-prompt-toggle-button");
  promptButton.click();
  await waitUntil(() => promptButton.textContent.includes("Enable"));

  checkTelemetryEvents([{
    method: "update_conn_prompt",
    extras: { "prompt_enabled": "false", "runtime_id": telemetryRuntimeId },
  }], sessionId);

  info("Click on the toggle button again and check we log the correct value");
  promptButton.click();
  await waitUntil(() => promptButton.textContent.includes("Disable"));

  checkTelemetryEvents([{
    method: "update_conn_prompt",
    extras: { "prompt_enabled": "true", "runtime_id": telemetryRuntimeId },
  }], sessionId);

  info("Open the profiler dialog");
  await openProfilerDialog(usbClient, document);

  checkTelemetryEvents([{
    method: "show_profiler",
    extras: { "runtime_id": telemetryRuntimeId },
  }], sessionId);

  info("Remove runtime");
  mocks.removeRuntime(RUNTIME_ID);
  mocks.emitUSBUpdate();
  await waitUntil(() => !findSidebarItemByText(RUNTIME_NAME, document));

  await removeTab(tab);
});
