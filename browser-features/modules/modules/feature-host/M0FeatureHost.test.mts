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
  isM0FixtureCompatibleForTests,
  m0PayloadEvaluationsForTests,
  type M0TestHostHandle,
} from "./FeatureHost.sys.mts";
import { type FixtureVersion, M0_FIXTURES } from "./M0Fixtures.sys.mts";
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

type JsonRecord = Record<string, unknown>;

function recordCopy(session: M0TestHostHandle): JsonRecord {
  return JSON.parse(JSON.stringify(session.host.diagnostics)) as JsonRecord;
}

function recordSnapshot(record: JsonRecord): JsonRecord {
  return {
    selectedVersion: record.selectedVersion,
    selectedDigest: record.selectedDigest,
    selectedHealth: record.selectedHealth,
    selectedHealthBuild: record.selectedHealthBuild,
    rollbackVersion: record.rollbackVersion,
    rollbackDigest: record.rollbackDigest,
    rollbackHealthBuild: record.rollbackHealthBuild,
    desiredEnabled: record.desiredEnabled,
  };
}

function operationRequest(
  op: string,
  expectedRevision: number | null,
  target: FixtureVersion | null,
  recoveryEpoch: string | null = null,
): JsonRecord {
  return {
    featureId: "experimental-local-sample",
    op,
    expectedRevision,
    target,
    recoveryEpoch,
    normalizedInput: op === "startup_recovery"
      ? `startup_recovery:${recoveryEpoch}`
      : `${op}:${target ?? "none"}`,
  };
}

async function seedRecord(root: string, record: JsonRecord): Promise<void> {
  const path = PathUtils.join(root, "state.json");
  await IOUtils.writeJSON(path, record, {
    tmpPath: `${path}.seed.tmp`,
    flush: true,
  });
}

async function finishTestHosts(
  first: M0TestHostHandle | null,
  second: M0TestHostHandle | null = null,
): Promise<void> {
  if (second) {
    const result = await second.shutdownForRestartForTests();
    assertEquals(result.errors.length, 0, "reopened host stops cleanly");
    await second.cleanup();
  }
  if (first) {
    const result = await first.shutdownForRestartForTests();
    assertEquals(result.errors.length, 0, "test host stops cleanly");
    await first.cleanup();
  }
}

async function testDisabledUpdateCrashRecovery(): Promise<void> {
  let first: M0TestHostHandle | null = null;
  let reopened: M0TestHostHandle | null = null;
  try {
    first = await createM0FeatureHostForTests(true);
    await first.host.install("recovery-install", 0, "v1");
    first.failNextWriteForTests("after_replace");
    const failure = await first.host.update(
      "disabled-update-crash",
      first.host.diagnostics.stateRevision,
      "v2",
    );
    assertEquals(
      failure.status,
      "storage_failed",
      "uncertain PREPARED write fails closed",
    );
    const root = first.root;
    await first.shutdownForRestartForTests();
    reopened = await createM0FeatureHostForTests(true, root, true);
    assertEquals(
      reopened.host.evaluationCountForTests,
      0,
      "disabled update recovery does not evaluate",
    );
    assertEquals(
      reopened.host.diagnostics.selectedVersion,
      "v1",
      "disabled recovery restores before pointer",
    );
    assertEquals(
      reopened.host.diagnostics.desiredEnabled,
      false,
      "disabled recovery remains disabled",
    );
    assertEquals(
      reopened.host.diagnostics.pending,
      null,
      "recovery clears pending",
    );
    const recovered = reopened.host.queryOutcome("disabled-update-crash");
    assertEquals(
      recovered === "outcome_unknown_or_expired" ? null : recovered.status,
      "failed_restored",
      "original transaction receives restored outcome",
    );
  } finally {
    await finishTestHosts(first, reopened);
  }
}

async function testNormalStartupAndInterruptedUpdateRecovery(): Promise<void> {
  let first: M0TestHostHandle | null = null;
  let reopened: M0TestHostHandle | null = null;
  try {
    first = await createM0FeatureHostForTests(true);
    await first.host.install("startup-install", 0, "v1");
    await first.host.enable(
      "startup-enable",
      first.host.diagnostics.stateRevision,
    );
    const root = first.root;
    await first.shutdownForRestartForTests();
    reopened = await createM0FeatureHostForTests(true, root, true);
    assertEquals(
      reopened.host.evaluationCountForTests,
      1,
      "normal startup evaluates selected once in a fresh host",
    );
    assertEquals(
      reopened.host.runningLifecycleForTests?.health(),
      "v1:1",
      "startup uses a fresh generation",
    );
    assertEquals(
      reopened.host.diagnostics.attempt,
      null,
      "terminal startup clears its episode",
    );
    await reopened.shutdownForRestartForTests();

    const state = recordCopy(reopened);
    const request = operationRequest(
      "update",
      Number(state.stateRevision),
      "v2",
    );
    const txId = "interrupted-enabled-update";
    const before = recordSnapshot(state);
    const targetDigest = M0_FIXTURES.v2.digest;
    state.pending = {
      op: "update",
      txId,
      request,
      before,
      targetVersion: "v2",
      targetDigest,
      phase: "CANDIDATE_READY",
    };
    state.attempt = {
      recoveryEpoch: "interrupted-update-episode",
      originTxId: txId,
      request,
      targetDigest,
      attemptedDigests: [targetDigest],
      rejectedDigests: [],
    };
    state.stateRevision = Number(state.stateRevision) + 1;
    await seedRecord(root, state);
    reopened = await createM0FeatureHostForTests(true, root, true);
    assertEquals(
      reopened.host.evaluationCountForTests,
      1,
      "interrupted update restores only the verified before generation",
    );
    assertEquals(
      reopened.host.runningLifecycleForTests?.health(),
      "v1:1",
      "interrupted target is not resumed",
    );
    assertEquals(
      reopened.host.diagnostics.selectedVersion,
      "v1",
      "before selection is restored",
    );
    assertEquals(
      reopened.host.diagnostics.lastOutcome?.status,
      "failed_restored",
      "origin update ends restored",
    );
    assertEquals(
      reopened.host.diagnostics.attempt,
      null,
      "restored terminal clears episode",
    );
  } finally {
    await finishTestHosts(first, reopened);
  }
}

async function testAttemptBudgetAndBuildMismatch(): Promise<void> {
  let first: M0TestHostHandle | null = null;
  let reopened: M0TestHostHandle | null = null;
  try {
    first = await createM0FeatureHostForTests(true);
    await first.host.install("budget-install", 0, "v1");
    await first.host.enable(
      "budget-enable",
      first.host.diagnostics.stateRevision,
    );
    const v1Proof = first.host.diagnostics.selectedHealthBuild;
    await first.host.update(
      "budget-update-v2",
      first.host.diagnostics.stateRevision,
      "v2",
    );
    await first.host.rollback(
      "budget-rollback-v1",
      first.host.diagnostics.stateRevision,
    );
    const root = first.root;
    await first.shutdownForRestartForTests();
    const state = recordCopy(first);
    const txId = "startup-budget-episode";
    const epoch = "startup-budget-episode";
    const request = operationRequest("startup_recovery", null, null, epoch);
    const before = recordSnapshot(state);
    const proof = v1Proof as JsonRecord;
    state.pending = {
      op: "startup_recovery",
      txId,
      request,
      before: {
        ...before,
      },
      targetVersion: "v1",
      targetDigest: M0_FIXTURES.v1.digest,
      phase: "OLD_STOPPED",
    };
    state.attempt = {
      recoveryEpoch: epoch,
      originTxId: txId,
      request,
      targetDigest: M0_FIXTURES.v1.digest,
      attemptedDigests: [M0_FIXTURES.v1.digest],
      rejectedDigests: [],
    };
    state.stateRevision = Number(state.stateRevision) + 1;
    await seedRecord(root, state);
    reopened = await createM0FeatureHostForTests(true, root, true);
    assertEquals(
      reopened.host.evaluationCountForTests,
      1,
      "attempted selected is skipped in favor of one eligible fallback",
    );
    assertEquals(
      reopened.host.runningLifecycleForTests?.health(),
      "v2:1",
      "fallback generation is active",
    );
    assertEquals(
      reopened.host.diagnostics.selectedVersion,
      "v2",
      "fallback selection is committed",
    );
    assertEquals(
      reopened.host.diagnostics.rollbackVersion,
      null,
      "consumed fallback proof is cleared",
    );

    await reopened.shutdownForRestartForTests();
    const exhausted = recordCopy(reopened);
    exhausted.lastOutcome = {
      txId,
      request,
      status: "committed_pending_publish",
      selectedVersion: "v2",
    };
    exhausted.attempt = {
      recoveryEpoch: epoch,
      originTxId: txId,
      request,
      targetDigest: M0_FIXTURES.v2.digest,
      attemptedDigests: [M0_FIXTURES.v1.digest, M0_FIXTURES.v2.digest],
      rejectedDigests: [],
    };
    exhausted.rollbackVersion = "v1";
    exhausted.rollbackDigest = M0_FIXTURES.v1.digest;
    exhausted.rollbackHealthBuild = proof;
    exhausted.stateRevision = Number(exhausted.stateRevision) + 1;
    await seedRecord(root, exhausted);
    reopened = await createM0FeatureHostForTests(true, root, true);
    assertEquals(
      reopened.host.evaluationCountForTests,
      0,
      "exhausted episode is quarantined without repeating a digest",
    );
    assertEquals(
      reopened.host.diagnostics.desiredEnabled,
      true,
      "quarantine preserves enabled intent",
    );
    assert(
      reopened.host.diagnostics.blockedReason !== null,
      "attempt exhaustion remains durably blocked",
    );
    await reopened.shutdownForRestartForTests();
    reopened = await createM0FeatureHostForTests(true, root, true);
    assertEquals(
      reopened.host.evaluationCountForTests,
      0,
      "persistent quarantine is not automatically cleared on another open",
    );
    assert(
      reopened.host.diagnostics.blockedReason !== null,
      "blocked reason remains durable across repeated open",
    );
  } finally {
    await finishTestHosts(first, reopened);
  }
}

async function testActualPendingPhaseRecovery(): Promise<void> {
  for (
    const [phase, skip, expectedEvaluations] of [
      ["PREPARED", 0, 1],
      ["OLD_STOPPED", 1, 1],
      ["CANDIDATE_READY", 3, 2],
    ] as const
  ) {
    let first: M0TestHostHandle | null = null;
    let reopened: M0TestHostHandle | null = null;
    try {
      first = await createM0FeatureHostForTests(true);
      await first.host.install(`phase-install-${phase}`, 0, "v1");
      await first.host.enable(
        `phase-enable-${phase}`,
        first.host.diagnostics.stateRevision,
      );
      const evaluationsBefore = m0PayloadEvaluationsForTests();
      first.failNextWriteForTests("after_replace", skip);
      const result = await first.host.update(
        `phase-update-${phase}`,
        first.host.diagnostics.stateRevision,
        "v2",
      );
      assertEquals(
        result.status,
        "storage_failed",
        `${phase} write uncertainty is reported`,
      );
      const persisted = await IOUtils.readJSON(
        PathUtils.join(first.root, "state.json"),
      ) as JsonRecord;
      assertEquals(
        (persisted.pending as JsonRecord).phase,
        phase,
        `${phase} phase is the actual replaced journal state`,
      );
      const root = first.root;
      await first.shutdownForRestartForTests();
      reopened = await createM0FeatureHostForTests(true, root, true);
      assertEquals(
        reopened.host.diagnostics.selectedVersion,
        "v1",
        `${phase} recovery restores the before selection`,
      );
      assertEquals(
        reopened.host.diagnostics.lastOutcome?.status,
        "failed_restored",
        `${phase} recovery terminates the original transaction`,
      );
      assertEquals(
        m0PayloadEvaluationsForTests() - evaluationsBefore,
        expectedEvaluations,
        `${phase} has the expected candidate and restore evaluations`,
      );
    } finally {
      await finishTestHosts(first, reopened);
    }
  }
}

function testPinnedCompatibilityPreflight(): void {
  const evaluationsBefore = m0PayloadEvaluationsForTests();
  assert(
    isM0FixtureCompatibleForTests(
      "v1",
      "m0-schema-2",
      Services.appinfo.appBuildID,
      ["window-marker"],
    ),
    "pinned host/runtime/capability tuple is accepted",
  );
  assert(
    !isM0FixtureCompatibleForTests(
      "v1",
      "m0-schema-1",
      Services.appinfo.appBuildID,
      ["window-marker"],
    ),
    "unsupported host schema is rejected",
  );
  assert(
    !isM0FixtureCompatibleForTests(
      "v1",
      "m0-schema-2",
      `${Services.appinfo.appBuildID}-other`,
      ["window-marker"],
    ),
    "unsupported runtime build is rejected",
  );
  assert(
    !isM0FixtureCompatibleForTests(
      "v1",
      "m0-schema-2",
      Services.appinfo.appBuildID,
      [],
    ),
    "missing required capability is rejected",
  );
  assertEquals(
    m0PayloadEvaluationsForTests(),
    evaluationsBefore,
    "compatibility preflight performs no payload evaluation",
  );
}

async function testInvalidSelectedFallbackAndCorruptRecord(): Promise<void> {
  let first: M0TestHostHandle | null = null;
  let reopened: M0TestHostHandle | null = null;
  try {
    first = await createM0FeatureHostForTests(true);
    await first.host.install("invalid-install", 0, "v1");
    await first.host.enable(
      "invalid-enable",
      first.host.diagnostics.stateRevision,
    );
    await first.host.disable(
      "invalid-disable-to-cache-broken",
      first.host.diagnostics.stateRevision,
    );
    await first.host.update(
      "invalid-cache-broken",
      first.host.diagnostics.stateRevision,
      "broken",
    );
    await first.host.update(
      "invalid-restore-pointer",
      first.host.diagnostics.stateRevision,
      "v1",
    );
    await first.host.enable(
      "invalid-reenable-v1",
      first.host.diagnostics.stateRevision,
    );
    const root = first.root;
    await first.shutdownForRestartForTests();
    const state = recordCopy(first);
    const proof = state.selectedHealthBuild;
    state.selectedVersion = "broken";
    state.selectedDigest = M0_FIXTURES.broken.digest;
    state.selectedHealth = "unverified";
    state.selectedHealthBuild = null;
    state.rollbackVersion = "v1";
    state.rollbackDigest = M0_FIXTURES.v1.digest;
    state.rollbackHealthBuild = proof;
    state.stateRevision = Number(state.stateRevision) + 1;
    await seedRecord(root, state);
    reopened = await createM0FeatureHostForTests(true, root, true);
    assertEquals(
      reopened.host.evaluationCountForTests,
      2,
      "selected failure uses one fallback evaluation",
    );
    assertEquals(
      reopened.host.diagnostics.selectedVersion,
      "v1",
      "fallback becomes selected",
    );
    assertEquals(
      reopened.host.diagnostics.rollbackVersion,
      null,
      "failed selection is not promoted to rollback",
    );

    await reopened.shutdownForRestartForTests();
    const corrupt = {
      schemaVersion: 1,
      featureId: "experimental-local-sample",
    };
    await seedRecord(root, corrupt);
    const evaluations = m0PayloadEvaluationsForTests();
    let rejected = false;
    try {
      await createM0FeatureHostForTests(true, root, true);
    } catch {
      rejected = true;
    }
    assert(rejected, "incompatible schema is rejected");
    assertEquals(
      m0PayloadEvaluationsForTests(),
      evaluations,
      "corrupt record is rejected before payload evaluation",
    );
  } finally {
    await finishTestHosts(first, reopened);
  }
}

async function testPreflightRejectedDigestFallback(): Promise<void> {
  let first: M0TestHostHandle | null = null;
  let reopened: M0TestHostHandle | null = null;
  try {
    first = await createM0FeatureHostForTests(true);
    await first.host.install("preflight-install", 0, "v1");
    await first.host.enable(
      "preflight-enable",
      first.host.diagnostics.stateRevision,
    );
    await first.host.update(
      "preflight-update",
      first.host.diagnostics.stateRevision,
      "v2",
    );
    await first.host.rollback(
      "preflight-rollback",
      first.host.diagnostics.stateRevision,
    );
    const root = first.root;
    await first.shutdownForRestartForTests();
    await IOUtils.writeUTF8(
      PathUtils.join(root, "packages", `${M0_FIXTURES.v1.digest}.js`),
      "not the pinned fixture",
    );
    reopened = await createM0FeatureHostForTests(true, root, true);
    assertEquals(
      reopened.host.evaluationCountForTests,
      1,
      "preflight-rejected selected digest is never evaluated",
    );
    assertEquals(
      reopened.host.runningLifecycleForTests?.health(),
      "v2:1",
      "one independently verified fallback is activated",
    );
    assertEquals(
      reopened.host.diagnostics.selectedVersion,
      "v2",
      "fallback is durably selected",
    );
    assertEquals(
      reopened.host.diagnostics.rollbackVersion,
      null,
      "fallback consumes the stale pointer",
    );
  } finally {
    await finishTestHosts(first, reopened);
  }
}

async function testWriteFailureBoundaries(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  try {
    session = await createM0FeatureHostForTests(true);
    await session.host.install("write-install", 0, "v1");
    await session.host.enable(
      "write-enable",
      session.host.diagnostics.stateRevision,
    );
    const runtime = session.host.runningLifecycleForTests;
    const revision = session.host.diagnostics.stateRevision;
    let badTransactionRejected = false;
    try {
      await session.host.disable("", revision);
    } catch {
      badTransactionRejected = true;
    }
    assert(
      badTransactionRejected,
      "empty transaction ID is rejected before writes",
    );
    let unsafeRevisionRejected = false;
    try {
      await session.host.disable(
        "unsafe-revision",
        Number.MAX_SAFE_INTEGER + 1,
      );
    } catch {
      unsafeRevisionRejected = true;
    }
    assert(unsafeRevisionRejected, "unsafe revision is rejected before writes");
    assertEquals(
      session.host.diagnostics.stateRevision,
      revision,
      "invalid descriptors do not mutate state",
    );
    session.failNextWriteForTests("before_replace");
    const disabled = await session.host.disable(
      "disable-before-write",
      session.host.diagnostics.stateRevision,
    );
    assertEquals(
      disabled.status,
      "storage_failed",
      "pre-replace disable failure is reported",
    );
    assert(
      session.host.runningLifecycleForTests === runtime,
      "pre-replace disable failure leaves old runtime untouched",
    );
    assertEquals(runtime?.health(), "v1:1", "old generation remains callable");
  } finally {
    if (session) await finishTestHosts(session);
  }
}

async function testTerminalWriteUncertainty(): Promise<void> {
  for (const mode of ["before_replace", "after_replace"] as const) {
    let session: M0TestHostHandle | null = null;
    let reopened: M0TestHostHandle | null = null;
    try {
      session = await createM0FeatureHostForTests(true);
      await session.host.install(`terminal-install-${mode}`, 0, "v1");
      session.failNextWriteForTests(mode, 5);
      const result = await session.host.enable(
        `terminal-enable-${mode}`,
        session.host.diagnostics.stateRevision,
      );
      assertEquals(
        result.status,
        "storage_failed",
        `${mode} terminal write never reports success`,
      );
      assertEquals(
        session.host.runningLifecycleForTests,
        null,
        "uncertain terminal write stops runtime",
      );
      if (mode === "after_replace") {
        assertEquals(
          session.host.diagnostics.lastOutcome?.status,
          "succeeded",
          "durable success is preserved after replace error",
        );
        assertEquals(
          session.host.diagnostics.attempt,
          null,
          "durable terminal success cleared episode atomically",
        );
      } else {
        assertEquals(
          session.host.diagnostics.lastOutcome?.status,
          "failed_quarantined",
          "unfinished transaction is best-effort quarantined",
        );
        assert(
          session.host.diagnostics.attempt !== null,
          "quarantine retains attempt budget",
        );
        assertEquals(
          session.host.diagnostics.desiredEnabled,
          true,
          "quarantine preserves enabled intent",
        );
      }
      const root = session.root;
      await session.shutdownForRestartForTests();
      reopened = await createM0FeatureHostForTests(true, root, true);
      const durableOutcome = reopened.host.queryOutcome(
        `terminal-enable-${mode}`,
      );
      assertEquals(
        durableOutcome === "outcome_unknown_or_expired"
          ? null
          : durableOutcome.status,
        mode === "after_replace" ? "succeeded" : "failed_quarantined",
        `${mode} finalization state survives actual reopen`,
      );
      assertEquals(
        reopened.host.evaluationCountForTests,
        mode === "after_replace" ? 1 : 0,
        `${mode} reopen respects durable terminal or quarantine`,
      );
    } finally {
      await finishTestHosts(session, reopened);
    }
  }
}

async function testDurableDisableAndBuildMismatch(): Promise<void> {
  let first: M0TestHostHandle | null = null;
  let reopened: M0TestHostHandle | null = null;
  try {
    first = await createM0FeatureHostForTests(true);
    await first.host.install("disable-restart-install", 0, "v1");
    await first.host.enable(
      "disable-restart-enable",
      first.host.diagnostics.stateRevision,
    );
    const root = first.root;
    await first.host.disable(
      "disable-restart",
      first.host.diagnostics.stateRevision,
    );
    await first.shutdownForRestartForTests();
    reopened = await createM0FeatureHostForTests(true, root, true);
    assertEquals(
      reopened.host.evaluationCountForTests,
      0,
      "durable disable startup performs zero evaluation",
    );
    assertEquals(
      reopened.host.diagnostics.desiredEnabled,
      false,
      "durable disable remains authoritative",
    );
    await reopened.shutdownForRestartForTests();

    const state = recordCopy(reopened);
    state.desiredEnabled = true;
    state.selectedVersion = "v1";
    state.selectedDigest = M0_FIXTURES.v1.digest;
    state.selectedHealth = "unverified";
    state.selectedHealthBuild = null;
    state.rollbackVersion = "v2";
    state.rollbackDigest = M0_FIXTURES.v2.digest;
    state.rollbackHealthBuild = {
      hostBuildId: "old-host",
      runtimeBuildId: "old-runtime",
    };
    const request = operationRequest(
      "update",
      Number(state.stateRevision),
      "broken",
    );
    const txId = "build-mismatch-update";
    const targetDigest = M0_FIXTURES.broken.digest;
    state.pending = {
      op: "update",
      txId,
      request,
      before: recordSnapshot(state),
      targetVersion: "broken",
      targetDigest,
      phase: "OLD_STOPPED",
    };
    state.attempt = {
      recoveryEpoch: "build-mismatch-episode",
      originTxId: txId,
      request,
      targetDigest,
      attemptedDigests: [targetDigest],
      rejectedDigests: [],
    };
    state.stateRevision = Number(state.stateRevision) + 1;
    await seedRecord(root, state);
    reopened = await createM0FeatureHostForTests(true, root, true);
    assertEquals(
      reopened.host.evaluationCountForTests,
      0,
      "wrong-build before and rollback proofs are not evaluated",
    );
    assert(
      reopened.host.diagnostics.blockedReason !== null,
      "unrecoverable build mismatch is durably blocked",
    );
    assertEquals(
      reopened.host.diagnostics.desiredEnabled,
      true,
      "quarantine keeps desired-enabled for diagnosis",
    );
  } finally {
    await finishTestHosts(first, reopened);
  }
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
      name: "disabled update PREPARED crash restores before without evaluation",
      fn: testDisabledUpdateCrashRecovery,
    },
    {
      name: "normal startup and interrupted enabled update recovery",
      fn: testNormalStartupAndInterruptedUpdateRecovery,
    },
    {
      name: "durable attempt budget and repeated recovery quarantine",
      fn: testAttemptBudgetAndBuildMismatch,
    },
    {
      name: "actual replaced PREPARED OLD_STOPPED and CANDIDATE_READY recovery",
      fn: testActualPendingPhaseRecovery,
    },
    {
      name: "pinned host runtime and capability preflight",
      fn: testPinnedCompatibilityPreflight,
    },
    {
      name:
        "invalid selected startup falls back once and corrupt state is inert",
      fn: testInvalidSelectedFallbackAndCorruptRecord,
    },
    {
      name: "preflight-rejected digest is persisted before fallback",
      fn: testPreflightRejectedDigestFallback,
    },
    {
      name: "durable disable intent and current-build proof gate recovery",
      fn: testDurableDisableAndBuildMismatch,
    },
    {
      name: "terminal write uncertainty preserves durable success",
      fn: testTerminalWriteUncertainty,
    },
    {
      name: "known pre-replace disable failure preserves active runtime",
      fn: testWriteFailureBoundaries,
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
