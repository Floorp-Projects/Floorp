// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import {
  createM0FeatureHostForTests,
  hasUnfinishedM0FinalizationForTests,
  isEligibleM0MainWindowForTests,
  type M0TestHostHandle,
} from "./FeatureHost.sys.mts";
import { M0_FIXTURES } from "./M0Fixtures.sys.mts";
import {
  activeM0DeadlineTimersForTests,
  M0ModuleHost,
  ResourceScope,
} from "./M0ModuleHost.sys.mts";

async function testFreshGenerationLifecycle(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  try {
    session = await createM0FeatureHostForTests(true);
    const host = session.host;
    const install = await host.install("m0-install", 0, "v1");
    assertEquals(
      install.status,
      "succeeded",
      "install stores but does not run v1",
    );
    assertEquals(
      host.diagnostics.desiredEnabled,
      false,
      "install remains disabled",
    );
    assertEquals(
      host.runningLifecycleForTests,
      null,
      "install performs zero evaluation",
    );
    install.request.target = "broken";
    install.request.normalizedInput = "caller-mutation";
    install.selectedVersion = "broken";
    assertEquals(
      host.diagnostics.lastOutcome?.request.target,
      "v1",
      "initial operation result is detached from the canonical record",
    );
    assertEquals(
      host.diagnostics.lastOutcome?.selectedVersion,
      "v1",
      "outcome fields are detached from the canonical record",
    );
    const queried = host.queryOutcome("m0-install");
    assert(
      queried !== "outcome_unknown_or_expired",
      "install outcome is retained",
    );
    queried.request.target = "broken";
    assertEquals(
      host.diagnostics.lastOutcome?.request.target,
      "v1",
      "query result is detached from the canonical record",
    );

    const enableRevision = host.diagnostics.stateRevision;
    const firstEnable = host.enable("m0-enable-1", enableRevision);
    const queuedDuplicate = host.enable("m0-enable-1", enableRevision);
    const [enabled, duplicate] = await Promise.all([
      firstEnable,
      queuedDuplicate,
    ]);
    assertEquals(
      duplicate.status,
      "succeeded",
      "queued duplicate returns stored result",
    );
    assertEquals(
      enabled.status,
      "succeeded",
      "enable evaluates the selected fixture",
    );
    const first = host.runningLifecycleForTests;
    assert(first !== null, "enable starts a generation");
    assertEquals(first.health(), "v1:1", "fixture state starts fresh");
    await host.disable("m0-disable-1", host.diagnostics.stateRevision);
    assertThrows(() => first.health(), "nuked generation wrapper is inert");

    const reenabled = await host.enable(
      "m0-enable-2",
      host.diagnostics.stateRevision,
    );
    assertEquals(
      reenabled.status,
      "succeeded",
      "re-enable starts the selected fixture",
    );
    const second = host.runningLifecycleForTests;
    assert(
      second !== null && second !== first,
      "re-enable creates a fresh module instance",
    );
    assertEquals(
      second.health(),
      "v1:1",
      "same bytes have fresh mutable state",
    );
    const updated = await host.update(
      "m0-update-v2",
      host.diagnostics.stateRevision,
      "v2",
    );
    assertEquals(updated.status, "succeeded", "active update evaluates v2");
    assertEquals(
      host.runningLifecycleForTests?.health(),
      "v2:1",
      "update evaluates v2",
    );
    const rolledBack = await host.rollback(
      "m0-rollback-v1",
      host.diagnostics.stateRevision,
    );
    assertEquals(
      rolledBack.status,
      "succeeded",
      "rollback requires and uses current-build proof",
    );
    assertEquals(
      host.runningLifecycleForTests?.health(),
      "v1:1",
      "rollback loads v1 fresh",
    );
    assertEquals(host.diagnostics.selectedVersion, "v1", "rollback selects v1");
  } finally {
    if (session) {
      const report = await session.cleanup();
      assertEquals(report.errors.length, 0, "test host shuts down cleanly");
      assertEquals(report.timedOut, false, "test host cleanup completes");
    }
  }
}

async function testResourceScopeRevocation(): Promise<void> {
  const scope = new ResourceScope();
  const disposed: string[] = [];
  const old = scope.register(() => {
    disposed.push("old");
  });
  scope.revoke();
  let rejected = false;
  try {
    scope.register(() => {
      disposed.push("new");
    });
  } catch {
    rejected = true;
  }
  assert(rejected, "revoked scope rejects new registrations");
  await Promise.all([old.dispose(), old.dispose(), scope.disposeAll(100)]);
  assertEquals(disposed.join(","), "old", "cleanup handle is idempotent");
  const reentrantScope = new ResourceScope();
  let selfHandle: { dispose(): Promise<void> } | null = null;
  selfHandle = reentrantScope.register(async () => {
    disposed.push("self");
    await selfHandle?.dispose();
  });
  const report = await reentrantScope.disposeAll(100);
  assertEquals(
    report.errors.length,
    0,
    "reentrant self-disposal is a non-awaiting no-op",
  );
  assertEquals(disposed.join(","), "old,self", "reentrant cleanup runs once");
  const nestedScope = new ResourceScope();
  nestedScope.child("window").register(() => {});
  assertEquals(
    nestedScope.resourceCount,
    1,
    "resource count includes children",
  );
  assertEquals(nestedScope.childCount, 1, "child count includes nested scopes");
  await nestedScope.disposeAll(100);
  const delayedScope = new ResourceScope();
  const delayedControl: { finish?: () => void } = {};
  const delayed = delayedScope.register(() =>
    new Promise<void>((resolve) => {
      delayedControl.finish = resolve;
    })
  );
  const delayedDisposal = delayed.dispose();
  const timerBaseline = activeM0DeadlineTimersForTests();
  const timeoutReport = await delayedScope.disposeAll(1);
  assertEquals(
    timeoutReport.timedOut,
    true,
    "drain reports pending manual disposal",
  );
  assert(
    timeoutReport.errors.includes("resource cleanup deadline"),
    "drain reports its deadline instead of clean success",
  );
  assertEquals(
    activeM0DeadlineTimersForTests(),
    timerBaseline,
    "resource deadline timer is cleared after timeout",
  );
  delayedControl.finish?.();
  await delayedDisposal;
  const failedScope = new ResourceScope();
  const manuallyFailing = failedScope.register(() => {
    throw new Error("manual disposer failure");
  });
  let manualFailureObserved = false;
  try {
    await manuallyFailing.dispose();
  } catch {
    manualFailureObserved = true;
  }
  assert(manualFailureObserved, "manual disposer rejects on cleanup failure");
  const failedDrain = await failedScope.disposeAll(100);
  assert(
    failedDrain.errors.some((error) =>
      error.includes("manual disposer failure")
    ),
    "drain includes prior manual disposal failures",
  );
}

async function testFailedEnableKeepsDisabledSelection(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  try {
    session = await createM0FeatureHostForTests(true);
    const host = session.host;
    await host.install("m0-broken-install", 0, "broken");
    const failed = await host.enable(
      "m0-broken-enable",
      host.diagnostics.stateRevision,
    );
    assertEquals(
      failed.status,
      "failed_restored",
      `broken fixture is rejected: ${host.diagnostics.blockedReason ?? "none"}`,
    );
    assertEquals(
      host.diagnostics.desiredEnabled,
      false,
      "failed enable stays disabled",
    );
    assertEquals(
      host.diagnostics.selectedVersion,
      "broken",
      "selection is preserved",
    );
    assertEquals(
      host.runningLifecycleForTests,
      null,
      "failed fixture is not retained",
    );
  } finally {
    if (session) {
      const report = await session.cleanup();
      assertEquals(
        report.errors.length,
        0,
        "failed enable host shuts down cleanly",
      );
      assertEquals(report.timedOut, false, "failed enable cleanup completes");
    }
  }
}

function testWindowEligibilityAndFinalizationGuard(): void {
  assert(
    isEligibleM0MainWindowForTests("navigator:browser", true, false, false),
    "visible, non-private browser main window is eligible",
  );
  assert(
    !isEligibleM0MainWindowForTests("navigator:browser", false, false, false),
    "hidden-toolbar browser popup is excluded",
  );
  assert(
    !isEligibleM0MainWindowForTests("navigator:browser", true, false, true),
    "web panel is excluded",
  );
  assert(
    !isEligibleM0MainWindowForTests("navigator:browser", true, true, false),
    "private window is excluded",
  );
  assert(
    !isEligibleM0MainWindowForTests("navigator:popup", true, false, false),
    "non-browser popup is excluded",
  );
  assert(
    hasUnfinishedM0FinalizationForTests("tx", null, null, "tx"),
    "pending transaction is unfinished",
  );
  assert(
    hasUnfinishedM0FinalizationForTests(null, "tx", "pending", "tx"),
    "pending outcome is unfinished",
  );
  assert(
    !hasUnfinishedM0FinalizationForTests(null, "tx", "succeeded", "tx"),
    "durable success is preserved during finalization failure",
  );
  assert(
    !hasUnfinishedM0FinalizationForTests(null, "tx", "failed_restored", "tx"),
    "durable restoration is preserved during finalization failure",
  );
  assert(
    !hasUnfinishedM0FinalizationForTests("tx", "tx", "succeeded", "tx"),
    "terminal success takes precedence over stale pending metadata",
  );
  assert(
    !hasUnfinishedM0FinalizationForTests("tx", "tx", "failed_restored", "tx"),
    "terminal restoration takes precedence over stale pending metadata",
  );
}

async function testSingletonInitializationWaitsForOpen(): Promise<void> {
  const suffix = Services.uuid.generateUUID().toString().replace(/[{}]/g, "")
    .toLowerCase();
  const root = PathUtils.join(
    PathUtils.profileDir,
    "floorp-hotfix-m0-tests",
    `m0-${suffix}`,
  );
  const [first, second] = await Promise.all([
    createM0FeatureHostForTests(true, root),
    createM0FeatureHostForTests(true, root),
  ]);
  assert(first.host === second.host, "concurrent open calls share one owner");
  assertEquals(
    first.host.diagnostics.stateRevision,
    0,
    "open resolves after record load",
  );
  const report = await first.cleanup();
  assertEquals(
    report.errors.length,
    0,
    "singleton test host shuts down cleanly",
  );
}

async function testShutdownRejectsQueuedOperations(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  try {
    session = await createM0FeatureHostForTests(true);
    const host = session.host;
    await host.install("m0-shutdown-install", 0, "v1");
    const revision = host.diagnostics.stateRevision;
    const rollback = host.rollback("m0-shutdown-rollback", revision);
    const enable = host.enable("m0-shutdown-enable", revision);
    const shutdown = host.shutdown();
    const results = await Promise.allSettled([rollback, enable]);
    assert(
      results.every((result) =>
        result.status === "rejected" &&
        result.reason instanceof Error &&
        result.reason.message === "M0 host closed"
      ),
      "dequeued operations reject after shutdown starts",
    );
    const report = await shutdown;
    assertEquals(report.errors.length, 0, "queued shutdown drains cleanly");
    assertEquals(
      host.runningLifecycleForTests,
      null,
      "shutdown did not activate queued work",
    );
  } finally {
    if (session) await session.cleanup();
  }
}

async function testPreparedWriteFailurePreservesActiveWindowMap(): Promise<
  void
> {
  let session: M0TestHostHandle | null = null;
  try {
    session = await createM0FeatureHostForTests(true);
    const host = session.host;
    await host.install("m0-w0-install", 0, "v1");
    await host.enable("m0-w0-enable", host.diagnostics.stateRevision);
    const original = host.runningLifecycleForTests;
    assert(original !== null, "v1 is active before failed prepare");
    const windowsBefore = host.windowIdsForTests;
    assert(
      windowsBefore.length > 0,
      "native test captures an eligible main window",
    );
    const recordPath = PathUtils.join(session.root, "state.json");
    await IOUtils.remove(recordPath);
    await IOUtils.makeDirectory(recordPath, { permissions: 0o700 });
    const failed = await host.update(
      "m0-w0-failed-prepare",
      host.diagnostics.stateRevision,
      "v2",
    );
    assertEquals(
      failed.status,
      "storage_failed",
      "failed PREPARED write is reported",
    );
    assert(
      host.runningLifecycleForTests === original,
      "old generation remains active",
    );
    assertEquals(original.health(), "v1:1", "old generation remains usable");
    assertEquals(
      host.windowIdsForTests.join("|"),
      windowsBefore.join("|"),
      "failed PREPARED write leaves old broker window IDs unchanged",
    );
  } finally {
    if (session) await session.cleanup();
  }
}

async function testNativeFiftyUpdatesStayBounded(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  try {
    session = await createM0FeatureHostForTests(true);
    const host = session.host;
    await host.install("m0-50-install", 0, "v1");
    await host.enable("m0-50-enable", host.diagnostics.stateRevision);
    const baseline = host.runningScopeDiagnosticsForTests;
    assert(baseline.resources > 0, "native window resources are tracked");
    const timerBaseline = activeM0DeadlineTimersForTests();
    for (let cycle = 0; cycle < 50; cycle += 1) {
      const previous = host.runningLifecycleForTests;
      assert(previous !== null, "generation is active before each update");
      const target = cycle % 2 === 0 ? "v2" : "v1";
      const result = await host.update(
        `m0-50-update-${cycle}`,
        host.diagnostics.stateRevision,
        target,
      );
      assertEquals(
        result.status,
        "succeeded",
        `update ${cycle} succeeds; blockedReason=${
          host.diagnostics.blockedReason ?? "none"
        }`,
      );
      assertThrows(
        () => previous.health(),
        `update ${cycle} revokes prior wrapper`,
      );
      const current = host.runningLifecycleForTests;
      assert(current !== null, `update ${cycle} leaves a generation active`);
      assertEquals(
        current.health(),
        `${target}:1`,
        `update ${cycle} has fresh state`,
      );
      const diagnostics = host.runningScopeDiagnosticsForTests;
      assertEquals(
        diagnostics.resources,
        baseline.resources,
        "resource count stays bounded",
      );
      assertEquals(
        diagnostics.childScopes,
        baseline.childScopes,
        "window scopes stay bounded",
      );
      assertEquals(
        activeM0DeadlineTimersForTests(),
        timerBaseline,
        "lifecycle/disposal timers do not accumulate",
      );
    }
    const last = host.runningLifecycleForTests;
    assert(last !== null, "final generation remains active");
    await host.disable("m0-50-disable", host.diagnostics.stateRevision);
    assertThrows(() => last.health(), "disable revokes the final wrapper");
    assertEquals(
      host.runningScopeDiagnosticsForTests.resources,
      0,
      "disable drains resources",
    );
    assertEquals(
      activeM0DeadlineTimersForTests(),
      timerBaseline,
      "disable clears timers",
    );
  } finally {
    if (session) {
      const report = await session.cleanup();
      assertEquals(
        report.errors.length,
        0,
        "50-update host shuts down cleanly",
      );
      assertEquals(report.timedOut, false, "50-update cleanup completes");
    }
  }
}

async function testConcurrentModuleHostStopsShareReport(): Promise<void> {
  const fixture = M0_FIXTURES.v1;
  const runtime = new M0ModuleHost(
    "test-epoch",
    1,
    fixture.digest,
    fixture.source,
    () => JSON.stringify({ ok: true }),
  );
  await runtime.start([]);
  const first = runtime.stop("test-stop");
  const second = runtime.stop("test-stop");
  assert(first === second, "concurrent stop callers share one promise");
  const report = await first;
  assertEquals(
    report.errors.length,
    0,
    `shared stop report includes no cleanup errors: ${
      report.errors.join(" | ")
    }`,
  );
  assertEquals(report.timedOut, false, "shared stop report completes");
  assertEquals(
    runtime.lifecycleForTests,
    null,
    "stop releases the fixture wrapper",
  );
  assertEquals(
    runtime.scopeDiagnosticsForTests.resources,
    0,
    "stop clears resources",
  );
  assertEquals(
    runtime.scopeDiagnosticsForTests.childScopes,
    0,
    "stop clears child scopes",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "window eligibility and finalization guards",
      fn: testWindowEligibilityAndFinalizationGuard,
    },
    {
      name: "concurrent singleton initialization waits for open",
      fn: testSingletonInitializationWaitsForOpen,
    },
    {
      name: "shutdown rejects queued operations",
      fn: testShutdownRejectsQueuedOperations,
    },
    {
      name: "failed PREPARED write preserves active W0",
      fn: testPreparedWriteFailurePreservesActiveWindowMap,
    },
    {
      name: "native 50-update lifecycle remains bounded",
      fn: testNativeFiftyUpdatesStayBounded,
    },
    {
      name: "fresh sandbox lifecycle across enable/update/rollback",
      fn: testFreshGenerationLifecycle,
    },
    {
      name: "resource scope revokes synchronously and disposes once",
      fn: testResourceScopeRevocation,
    },
    {
      name: "failed enable preserves the disabled selected package",
      fn: testFailedEnableKeepsDisabledSelection,
    },
    {
      name: "concurrent module host stops share one report",
      fn: testConcurrentModuleHostStopsShareReport,
    },
  ];
  await runTests("M0FeatureHost.test.mts", tests);
}
