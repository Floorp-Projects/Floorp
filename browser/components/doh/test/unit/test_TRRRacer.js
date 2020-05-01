/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(setup);

add_task(async function test_TRRRacer_cleanRun() {
  let deferred = PromiseUtils.defer();
  let racer = new TRRRacer(() => {
    deferred.resolve();
    deferred.resolved = true;
  });
  racer.run();

  await deferred.promise;
  Assert.equal(racer._retryCount, 1);

  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    true
  ).parent;
  Assert.ok(events);
  events = events.filter(e => e[1] == "security.doh.trrPerformance");
  Assert.equal(events.length, racer._aggregator.totalLookups);

  Services.telemetry.clearEvents();

  // Simulate network changes and ensure no re-runs since it's already complete.
  async function testNetworkChange(captivePortal = false) {
    if (captivePortal) {
      Services.obs.notifyObservers(null, "captive-portal-login");
    } else {
      Services.obs.notifyObservers(null, "network:link-status-changed", "down");
    }

    Assert.ok(!racer._aggregator.aborted);

    if (captivePortal) {
      Services.obs.notifyObservers(null, "captive-portal-login-success");
    } else {
      Services.obs.notifyObservers(null, "network:link-status-changed", "up");
    }

    Assert.equal(racer._retryCount, 1);
    ensureNoTelemetry();

    if (captivePortal) {
      Services.obs.notifyObservers(null, "captive-portal-login-abort");
    }
  }

  testNetworkChange(false);
  testNetworkChange(true);
});

async function test_TRRRacer_networkFlux_helper(captivePortal = false) {
  let deferred = PromiseUtils.defer();
  let racer = new TRRRacer(() => {
    deferred.resolve();
    deferred.resolved = true;
  });
  racer.run();

  if (captivePortal) {
    Services.obs.notifyObservers(null, "captive-portal-login");
  } else {
    Services.obs.notifyObservers(null, "network:link-status-changed", "down");
  }

  Assert.ok(racer._aggregator.aborted);
  ensureNoTelemetry();
  Assert.equal(racer._retryCount, 1);
  Assert.ok(!deferred.resolved);

  if (captivePortal) {
    Services.obs.notifyObservers(null, "captive-portal-login-success");
  } else {
    Services.obs.notifyObservers(null, "network:link-status-changed", "up");
  }

  Assert.ok(!racer._aggregator.aborted);
  await deferred.promise;

  Assert.equal(racer._retryCount, 2);

  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    true
  ).parent;
  Assert.ok(events);
  events = events.filter(e => e[1] == "security.doh.trrPerformance");
  Assert.equal(events.length, racer._aggregator.totalLookups);

  Services.telemetry.clearEvents();
  if (captivePortal) {
    Services.obs.notifyObservers(null, "captive-portal-login-abort");
  }
}

add_task(async function test_TRRRacer_networkFlux() {
  await test_TRRRacer_networkFlux_helper(false);
  await test_TRRRacer_networkFlux_helper(true);
});

async function test_TRRRacer_maxRetries_helper(captivePortal = false) {
  let deferred = PromiseUtils.defer();
  let racer = new TRRRacer(() => {
    deferred.resolve();
    deferred.resolved = true;
  });
  racer.run();
  info("ran new racer");
  // Start at i = 1 since we're already at retry #1.
  for (let i = 1; i < 5; ++i) {
    if (captivePortal) {
      Services.obs.notifyObservers(null, "captive-portal-login");
    } else {
      Services.obs.notifyObservers(null, "network:link-status-changed", "down");
    }

    info("notified observers");

    Assert.ok(racer._aggregator.aborted);
    ensureNoTelemetry();
    Assert.equal(racer._retryCount, i);
    Assert.ok(!deferred.resolved);

    if (captivePortal) {
      Services.obs.notifyObservers(null, "captive-portal-login-success");
    } else {
      Services.obs.notifyObservers(null, "network:link-status-changed", "up");
    }
  }

  // Simulate a "down" network event and ensure we still send telemetry
  // since we've maxed out our retry count.
  if (captivePortal) {
    Services.obs.notifyObservers(null, "captive-portal-login");
  } else {
    Services.obs.notifyObservers(null, "network:link-status-changed", "down");
  }
  Assert.ok(!racer._aggregator.aborted);
  await deferred.promise;
  Assert.equal(racer._retryCount, 5);

  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    true
  ).parent;
  Assert.ok(events);
  events = events.filter(e => e[1] == "security.doh.trrPerformance");
  Assert.equal(events.length, racer._aggregator.totalLookups);

  Services.telemetry.clearEvents();
  if (captivePortal) {
    Services.obs.notifyObservers(null, "captive-portal-login-abort");
  }
}

add_task(async function test_TRRRacer_maxRetries() {
  await test_TRRRacer_maxRetries_helper(false);
  await test_TRRRacer_maxRetries_helper(true);
});
