/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of abandonment telemetry.
// - search_mode

add_setup(async function () {
  await initSearchModeTest();
});

add_task(async function not_search_mode() {
  await doNotSearchModeTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ search_mode: "" }]),
  });
});

add_task(async function search_engine() {
  await doSearchEngineTest({
    trigger: () => doBlur(),
    assert: () =>
      assertAbandonmentTelemetry([{ search_mode: "search_engine" }]),
  });
});

add_task(async function bookmarks() {
  await doBookmarksTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ search_mode: "bookmarks" }]),
  });
});

add_task(async function history() {
  await doHistoryTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ search_mode: "history" }]),
  });
});

add_task(async function tabs() {
  await doTabTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ search_mode: "tabs" }]),
  });
});

add_task(async function actions() {
  await doActionsTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ search_mode: "actions" }]),
  });
});
