/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of engagement telemetry.
// - search_engine_default_id

add_setup(async function () {
  await initSearchEngineDefaultIdTest();
});

add_task(async function basic() {
  await doSearchEngineDefaultIdTest({
    trigger: () => doEnter(),
    assert: engineId =>
      assertEngagementTelemetry([{ search_engine_default_id: engineId }]),
  });
});
