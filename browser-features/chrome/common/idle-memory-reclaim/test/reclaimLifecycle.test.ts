// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import IdleMemoryReclaim, { sanitizeSettings } from "../index.ts";
import { BYTES_PER_MB, DEFAULT_SETTINGS, DEFAULT_STATS } from "../types.ts";
import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";
import type { ReclaimTestInstance, SharedReclaimStats } from "./types.ts";

function deferred<T>() {
  let resolve!: (value: T) => void;
  const promise = new Promise<T>((done) => {
    resolve = done;
  });
  return { promise, resolve };
}

function fixture(shared: SharedReclaimStats = { value: { ...DEFAULT_STATS } }) {
  // Use the real request/teardown methods, replacing only service boundaries.
  // Calling the constructor would register observers on the user's browser.
  const instance = Object.create(
    IdleMemoryReclaim.prototype,
  ) as ReclaimTestInstance;
  let calls = 0;
  let snapshots = 0;
  Object.assign(instance, {
    settings: { ...DEFAULT_SETTINGS, enabled: true },
    reclaiming: false,
    disposed: false,
    pollTimerId: null,
    idleObserver: null,
    prefObserver: null,
    registeredIdleSec: null,
  });
  instance.takeSnapshot = () => {
    snapshots++;
    return Promise.resolve({
      residentBytes: 800 * BYTES_PER_MB,
      ghostWindows: 0,
    });
  };
  instance.isStillIdle = () => true;
  instance.loadStats = () => ({ ...shared.value });
  instance.saveStats = (stats) => {
    shared.value = { ...stats };
  };
  instance.runMinimizeMemoryUsage = () => {
    calls++;
    return Promise.resolve();
  };
  return { instance, shared, calls: () => calls, snapshots: () => snapshots };
}

async function testDefaultsRequireOptIn() {
  assertEquals(
    DEFAULT_SETTINGS.enabled,
    false,
    "feature must be off by default",
  );
  for (const raw of [null, {}, { enabled: "true" }]) {
    const f = fixture();
    f.instance.settings = sanitizeSettings(raw);
    await f.instance.reclaimIfNeeded(true);
    assertEquals(f.calls(), 0, "absent or invalid consent must not reclaim");
  }
  assertEquals(
    sanitizeSettings({ enabled: true }).enabled,
    true,
    "keep explicit opt-in",
  );
}

async function testStateChangesDuringSnapshot() {
  for (const change of ["active", "disabled", "disposed"]) {
    const f = fixture();
    const snapshot = deferred<
      { residentBytes: number; ghostWindows: number }
    >();
    f.instance.takeSnapshot = () => snapshot.promise;
    const pending = f.instance.reclaimIfNeeded(true);
    if (change === "active") f.instance.isStillIdle = () => false;
    if (change === "disabled") f.instance.settings.enabled = false;
    if (change === "disposed") f.instance.teardown();
    snapshot.resolve({ residentBytes: 800 * BYTES_PER_MB, ghostWindows: 0 });
    await pending;
    assertEquals(
      f.calls(),
      0,
      `${change} during snapshot must prevent reclaim`,
    );
    assertEquals(
      f.shared.value.lastRunAt,
      0,
      `${change} must not reserve the throttle`,
    );
    assertEquals(
      f.instance.reclaiming,
      false,
      `${change} must release the guard`,
    );
  }
}

async function testConcurrentRequestsInOneWindow() {
  const f = fixture();
  const snapshot = deferred<{ residentBytes: number; ghostWindows: number }>();
  f.instance.takeSnapshot = () => snapshot.promise;
  const first = f.instance.reclaimIfNeeded(true);
  await f.instance.reclaimIfNeeded(true);
  snapshot.resolve({ residentBytes: 800 * BYTES_PER_MB, ghostWindows: 0 });
  await first;
  assertEquals(
    f.calls(),
    1,
    "overlapping idle and poll requests must reclaim once",
  );
  assertEquals(f.shared.value.runCount, 1, "count only the completed run");
}

async function testConcurrentWindows() {
  const shared = { value: { ...DEFAULT_STATS } };
  const first = fixture(shared);
  const second = fixture(shared);
  await Promise.all([
    first.instance.reclaimIfNeeded(true),
    second.instance.reclaimIfNeeded(true),
  ]);
  assertEquals(
    first.calls() + second.calls(),
    1,
    "windows must share the throttle",
  );
  assertEquals(shared.value.runCount, 1, "shared stats must record one run");
}

async function testTeardownDuringReclaim() {
  const f = fixture();
  const started = deferred<void>();
  const completed = deferred<void>();
  f.instance.runMinimizeMemoryUsage = () => {
    started.resolve();
    return completed.promise;
  };
  const pending = f.instance.reclaimIfNeeded(true);
  await started.promise;
  f.instance.teardown();
  completed.resolve();
  await pending;
  assertEquals(
    f.snapshots(),
    1,
    "closed window must skip the follow-up measurement",
  );
  assertEquals(
    f.shared.value.runCount,
    1,
    "already started run must still be recorded",
  );
  assertEquals(f.instance.reclaiming, false, "teardown must release the guard");
  await f.instance.reclaimIfNeeded(true);
  assertEquals(f.snapshots(), 1, "disposed window must not start new work");
}

async function testDisableDuringReclaim() {
  const f = fixture();
  const started = deferred<void>();
  const completed = deferred<void>();
  f.instance.runMinimizeMemoryUsage = () => {
    started.resolve();
    return completed.promise;
  };
  const pending = f.instance.reclaimIfNeeded(true);
  await started.promise;
  f.instance.settings.enabled = false;
  completed.resolve();
  await pending;
  assertEquals(
    f.shared.value.runCount,
    1,
    "disabling cannot cancel an already started run",
  );
  f.shared.value.lastRunAt = 0;
  await f.instance.reclaimIfNeeded(true);
  assertEquals(
    f.shared.value.runCount,
    1,
    "disabled feature must not start another run",
  );
}

async function testFailureReleasesGuard() {
  const f = fixture();
  f.instance.runMinimizeMemoryUsage = () =>
    Promise.reject(new Error("test failure"));
  await f.instance.reclaimIfNeeded(true);
  assertEquals(
    f.instance.reclaiming,
    false,
    "failed reclaim must release the guard",
  );
  assertEquals(
    f.shared.value.runCount,
    0,
    "failed reclaim must not count as success",
  );
  f.shared.value.lastRunAt = 0;
  f.instance.runMinimizeMemoryUsage = () => Promise.resolve();
  await f.instance.reclaimIfNeeded(true);
  assertEquals(
    f.shared.value.runCount,
    1,
    "later requests must recover from failure",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("reclaimLifecycle.test.ts", [
    {
      name: "automatic reclaim requires explicit opt-in",
      fn: testDefaultsRequireOptIn,
    },
    {
      name: "state changes during snapshot prevent reclaim",
      fn: testStateChangesDuringSnapshot,
    },
    {
      name: "overlapping requests in one window reclaim once",
      fn: testConcurrentRequestsInOneWindow,
    },
    {
      name: "multiple windows share the reclaim throttle",
      fn: testConcurrentWindows,
    },
    {
      name:
        "teardown during reclaim preserves stats without further measurement",
      fn: testTeardownDuringReclaim,
    },
    {
      name: "disable during reclaim prevents future runs",
      fn: testDisableDuringReclaim,
    },
    {
      name: "failed reclaim releases the request guard",
      fn: testFailureReleasesGuard,
    },
  ]);
}
