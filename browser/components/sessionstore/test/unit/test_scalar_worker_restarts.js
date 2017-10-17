/*
 * The primary purpose of this test is to ensure that the sessionstore component
 * records information about erroneous workers into a scalar.
 */

"use strict";
const Telemetry = Services.telemetry;
const ScalarId = "browser.session.restore.worker_restart_count";

// Prepare the session file.
var profd = do_get_profile();
Cu.import("resource:///modules/sessionstore/SessionFile.jsm", this);

/**
 * In order to use browser.session.restore.worker_restart_count scalar, it has
 * to be registered in "toolkit/components/telemetry/Scalars.yaml".
 * This test ensures that the scalar is registered and empty.
 */
add_task(async function test_ensure_scalar_is_empty() {
  const scalars = Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT, false).parent || {};
  Assert.ok(!(ScalarId in scalars), "Sanity check; no scalars should be there yet.");
});

/**
 * Makes sure that the scalar is positively updated when amount of failures
 * becomes higher than the threshold.
 */
add_task(async function test_worker_restart() {
  let backstagePass = Cu.import("resource:///modules/sessionstore/SessionFile.jsm", {});
  backstagePass.SessionFileInternal._workerHealth.failures = backstagePass.kMaxWriteFailures + 1;
  backstagePass.SessionFileInternal._checkWorkerHealth();

  Assert.equal(backstagePass.SessionFileInternal._workerHealth.failures, 0,
    "Worker failure count should've been reset.");

  // Checking if the scalar is positively updated.
  const scalars = Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT, false).parent;
  Assert.equal(scalars[ScalarId], 1, "Should be increased with one hit.");
});
