/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that actions to change the RDM viewport size and rotation are logged to telemetry.
 */

const TEST_URL = "data:text/html;charset=utf-8,browser_telemetry_viewport_change.js";
const TELEMETRY_SCALAR_VIEWPORT_CHANGE_COUNT =
  "devtools.responsive.viewport_change_count";

addRDMTask(TEST_URL, async function({ ui, manager }) {
  info("Clear any existing Telemetry scalars");
  Services.telemetry.clearScalars();

  info("Resize the viewport");
  await setViewportSize(ui, manager, 100, 300);

  info("Rotate the viewport");
  rotateViewport(ui);

  const scalars = Services.telemetry.getSnapshotForScalars("main", false);
  ok(scalars.parent, "Telemetry scalars present");

  const count = scalars.parent[TELEMETRY_SCALAR_VIEWPORT_CHANGE_COUNT];
  is(count, 2, "Scalar has correct number of viewport changes logged");
});
