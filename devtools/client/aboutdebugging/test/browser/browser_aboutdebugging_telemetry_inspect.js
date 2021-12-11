/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-telemetry.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-telemetry.js",
  this
);

const TAB_URL = "data:text/html,<title>TEST_TAB</title>";

/**
 * Check that telemetry events are recorded when inspecting a target.
 */
add_task(async function() {
  setupTelemetryTest();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const sessionId = getOpenEventSessionId();
  ok(!isNaN(sessionId), "Open event has a valid session id");

  info("Open a new background tab TEST_TAB");
  const backgroundTab1 = await addTab(TAB_URL, { background: true });

  info("Wait for the tab to appear in the debug targets with the correct name");
  await waitUntil(() => findDebugTargetByText("TEST_TAB", document));

  const { devtoolsTab } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    "TEST_TAB"
  );

  const evts = readAboutDebuggingEvents().filter(e => e.method === "inspect");
  is(evts.length, 1, "Exactly one Inspect event found");
  is(
    evts[0].extras.target_type,
    "TAB",
    "Inspect event has the expected target type"
  );
  is(
    evts[0].extras.runtime_type,
    "this-firefox",
    "Inspect event has the expected runtime type"
  );
  is(
    evts[0].extras.session_id,
    sessionId,
    "Inspect event has the expected session"
  );

  info("Close the about:devtools-toolbox tab");
  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await waitForAboutDebuggingRequests(window.AboutDebugging.store);

  info("Remove first background tab");
  await removeTab(backgroundTab1);
  await waitUntil(() => !findDebugTargetByText("TEST_TAB", document));
  await waitForAboutDebuggingRequests(window.AboutDebugging.store);

  await removeTab(tab);
});
