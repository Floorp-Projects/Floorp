/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
/* import-globals-from helper-telemetry.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-telemetry.js",
  this
);

/**
 * Check that telemetry events are recorded when navigating between different
 * about:debugging pages.
 */
add_task(async function() {
  // enable USB devices mocks
  const mocks = new Mocks();

  setupTelemetryTest();

  const { tab, document, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const sessionId = getOpenEventSessionId();
  ok(!isNaN(sessionId), "Open event has a valid session id");

  info("Navigate to 'Connect' page");
  document.location.hash = "#/connect";
  await waitUntil(() => document.querySelector(".qa-connect-page"));
  checkSelectPageEvent("connect", sessionId);

  info("Navigate to 'USB device runtime' page");
  await navigateToUSBRuntime(mocks, document);
  checkSelectPageEvent("runtime", sessionId);
  await waitForRequestsToSettle(window.AboutDebugging.store);

  await removeTab(tab);
});

function checkSelectPageEvent(expectedType, expectedSessionId) {
  const evts = readAboutDebuggingEvents().filter(
    e => e.method === "select_page"
  );
  is(evts.length, 1, "Exactly one select_page event recorded");
  is(
    evts[0].extras.page_type,
    expectedType,
    "Select page event has the expected type"
  );
  is(
    evts[0].extras.session_id,
    expectedSessionId,
    "Select page event has the expected session"
  );
}

async function navigateToUSBRuntime(mocks, doc) {
  mocks.createUSBRuntime("1337id", {
    deviceName: "Fancy Phone",
    name: "Lorem ipsum",
  });
  mocks.emitUSBUpdate();
  await connectToRuntime("Fancy Phone", doc);
  // navigate to it via URL
  doc.location.hash = "#/runtime/1337id";
  await waitUntil(() => doc.querySelector(".qa-runtime-page"));
}
