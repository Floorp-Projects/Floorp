/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the following data of abandonment telemetry.
// - interaction

add_setup(async function () {
  await initInteractionTest();
});

add_task(async function topsites() {
  await doTopsitesTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ interaction: "topsites" }]),
  });
});

add_task(async function typed() {
  await doTypedTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ interaction: "typed" }]),
  });

  await doTypedWithResultsPopupTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ interaction: "typed" }]),
  });
});

add_task(async function pasted() {
  await doPastedTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ interaction: "pasted" }]),
  });

  await doPastedWithResultsPopupTest({
    trigger: () => doBlur(),
    assert: () => assertAbandonmentTelemetry([{ interaction: "pasted" }]),
  });
});

add_task(async function topsite_search() {
  // TODO: https://bugzilla.mozilla.org/show_bug.cgi?id=1804010
  // assertAbandonmentTelemetry([{ interaction: "topsite_search" }]);
});

add_task(async function returned_restarted_refined() {
  await doReturnedRestartedRefinedTest({
    trigger: () => doBlur(),
    assert: expected =>
      assertAbandonmentTelemetry([
        { interaction: "typed" },
        { interaction: expected },
      ]),
  });
});
