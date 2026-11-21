// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import type { M0TestHostHandle } from "./FeatureHost.sys.mts";
import { type FixtureVersion, M0_FIXTURES } from "./M0Fixtures.sys.mts";
import type { M0Lifecycle } from "./M0ModuleHost.sys.mts";

type FeatureHostModule = typeof import("./FeatureHost.sys.mts");
const {
  createM0FeatureHostForTests,
  hasUnfinishedM0FinalizationForTests,
  isEligibleM0MainWindowForTests,
  isM0FixtureCompatibleForTests,
  m0PayloadEvaluationsForTests,
} = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs",
) as FeatureHostModule;
type M0ModuleHostModule = typeof import("./M0ModuleHost.sys.mts");
const {
  activeM0DeadlineTimersForTests,
  M0ModuleHost,
  ResourceScope,
} = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/M0ModuleHost.sys.mjs",
) as M0ModuleHostModule;
const { BrowserWindowTracker } = ChromeUtils.importESModule(
  "resource:///modules/BrowserWindowTracker.sys.mjs",
);
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

function within<T>(promise: Promise<T>, milliseconds: number, label: string) {
  return new Promise<T>((resolve, reject) => {
    const timer = setTimeout(
      () => reject(new Error(`${label} timed out`)),
      milliseconds,
    );
    promise.then(
      (value) => {
        clearTimeout(timer);
        resolve(value);
      },
      (error: unknown) => {
        clearTimeout(timer);
        reject(error);
      },
    );
  });
}

function errorMessage(error: unknown): string | null {
  if (typeof error !== "object" || error === null || !("message" in error)) {
    return null;
  }
  return String(error.message);
}

function openTestBrowserWindow(
  features = "width=640,height=480",
  privateBrowsing = false,
): Window {
  const args = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString,
  );
  args.data = "about:blank";
  return BrowserWindowTracker.openWindow({
    args,
    features,
    remote: true,
    fission: true,
    private: privateBrowsing,
  }) as Window;
}

function openTestPopupWindow(): Window {
  const parent = Services.wm.getMostRecentWindow("navigator:browser");
  if (!parent) throw new Error("test runner browser window unavailable");
  const args = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString,
  );
  args.data = "about:blank";
  return Services.ww.openWindow(
    parent,
    "chrome://browser/content/browser.xhtml",
    "_blank",
    "chrome,popup,dialog=no,toolbar=no,menubar=no,location=no,width=640,height=480",
    args,
  ) as Window;
}

async function waitForDelayedStartup(window: Window): Promise<void> {
  if (
    (window as Window & {
      gBrowserInit?: { delayedStartupFinished?: boolean };
    }).gBrowserInit?.delayedStartupFinished
  ) return;
  await new Promise<void>((resolve, reject) => {
    let timer: number | null = null;
    const observer: nsIObserver = {
      observe: (subject: nsISupports | null) => {
        if (subject !== window) return;
        Services.obs.removeObserver(
          observer,
          "browser-delayed-startup-finished",
        );
        if (timer !== null) clearTimeout(timer);
        resolve();
      },
    };
    timer = setTimeout(() => {
      Services.obs.removeObserver(observer, "browser-delayed-startup-finished");
      reject(new Error("browser delayed startup timed out"));
    }, 30_000);
    Services.obs.addObserver(observer, "browser-delayed-startup-finished");
  });
}

async function waitForCondition(
  condition: () => boolean,
  label: string,
  milliseconds = 10_000,
): Promise<void> {
  const deadline = Date.now() + milliseconds;
  while (!condition()) {
    if (Date.now() >= deadline) throw new Error(`${label} timed out`);
    await new Promise<void>((resolve) => setTimeout(resolve, 25));
  }
}

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
    const repeatedUpdate = await host.update(
      "m0-update-v2-same-digest",
      host.diagnostics.stateRevision,
      "v2",
    );
    assertEquals(
      repeatedUpdate.status,
      "succeeded",
      "updating the active digest succeeds",
    );
    assertEquals(
      host.diagnostics.rollbackVersion,
      "v1",
      "same-digest update retains the distinct healthy rollback target",
    );
    assert(
      host.diagnostics.rollbackDigest !== host.diagnostics.selectedDigest,
      "same-digest update never points rollback at the selected digest",
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
    assertEquals(
      host.diagnostics.rollbackVersion,
      "v2",
      "normal rollback retains the displaced healthy selection",
    );
  } finally {
    if (session) {
      const report = await session.cleanup();
      assertEquals(
        report.errors.length,
        0,
        `test host shuts down cleanly: ${JSON.stringify(report.errors)}`,
      );
      assertEquals(report.timedOut, false, "test host cleanup completes");
    }
  }
}

async function testDisabledUpdateToRollbackKeepsDistinctTarget(): Promise<
  void
> {
  let session: M0TestHostHandle | null = null;
  try {
    session = await createM0FeatureHostForTests(true);
    session.excludeWindowsForTests();
    const host = session.host;
    await host.install("disabled-rollback-install", 0, "v1");
    await host.enable(
      "disabled-rollback-enable",
      host.diagnostics.stateRevision,
    );
    await host.update(
      "disabled-rollback-update-v2",
      host.diagnostics.stateRevision,
      "v2",
    );
    await host.disable(
      "disabled-rollback-disable",
      host.diagnostics.stateRevision,
    );
    assertEquals(host.diagnostics.selectedVersion, "v2", "v2 remains selected");
    assertEquals(host.diagnostics.rollbackVersion, "v1", "v1 is rollback");

    const evaluationsBefore = host.evaluationCountForTests;
    const updated = await host.update(
      "disabled-rollback-update-v1",
      host.diagnostics.stateRevision,
      "v1",
    );
    assertEquals(
      updated.status,
      "succeeded",
      "disabled update selects rollback v1",
    );
    assertEquals(host.diagnostics.selectedVersion, "v1", "v1 becomes selected");
    assertEquals(
      host.diagnostics.rollbackVersion,
      "v2",
      "healthy prior v2 replaces the now-selected rollback pointer",
    );
    assert(
      host.diagnostics.rollbackDigest !== host.diagnostics.selectedDigest,
      "disabled update leaves a distinct rollback digest",
    );
    assertEquals(
      host.evaluationCountForTests,
      evaluationsBefore,
      "disabled update performs no payload evaluation",
    );

    const rolledBack = await host.rollback(
      "disabled-rollback-use-v2",
      host.diagnostics.stateRevision,
    );
    assertEquals(rolledBack.status, "succeeded", "rollback selects v2");
    assertEquals(
      host.diagnostics.selectedVersion,
      "v2",
      "rollback is not a no-op",
    );
    assertEquals(
      host.diagnostics.rollbackVersion,
      null,
      "rollback consumes the pointer back to its just-selected version",
    );
    assertEquals(
      host.evaluationCountForTests,
      evaluationsBefore,
      "disabled rollback performs no payload evaluation",
    );
  } finally {
    if (session) await finishTestHosts(session);
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

  const concurrentScope = new ResourceScope();
  let startCleanup!: () => void;
  let finishCleanup!: () => void;
  const started = new Promise<void>((resolve) => startCleanup = resolve);
  const finished = new Promise<void>((resolve) => finishCleanup = resolve);
  let releaseCleanup!: () => void;
  const cleanupGate = new Promise<void>((resolve) => releaseCleanup = resolve);
  const concurrentHandle = concurrentScope.register(async () => {
    startCleanup();
    await cleanupGate;
    finishCleanup();
    throw new Error("concurrent manual disposal failure");
  });
  const originalDisposal = concurrentHandle.dispose().catch(() => {});
  await started;
  let drainSettled = false;
  const concurrentDrain = concurrentScope.disposeAll(100).then((value) => {
    drainSettled = true;
    return value;
  });
  await Promise.resolve();
  assert(!drainSettled, "root drain awaits the original manual disposal");
  releaseCleanup();
  await finished;
  const concurrentReport = await concurrentDrain;
  await originalDisposal;
  assert(
    concurrentReport.errors.some((error) =>
      error.includes("concurrent manual disposal failure")
    ),
    "root drain reports concurrent manual disposal failure",
  );

  const failedChildRoot = new ResourceScope();
  failedChildRoot.child("closed").register(() => {
    throw new Error("closed child cleanup failure");
  });
  const childReport = await failedChildRoot.disposeChild("closed", 100);
  assert(
    childReport.errors.some((error) =>
      error.includes("closed child cleanup failure")
    ),
    "child disposal reports its failure",
  );
  const failedChildDrain = await failedChildRoot.disposeAll(100);
  assert(
    failedChildDrain.errors.some((error) =>
      error.includes("closed child cleanup failure")
    ),
    "root retains completed child disposal failures",
  );

  const pendingChildRoot = new ResourceScope();
  let startChildCleanup!: () => void;
  let finishChildCleanup!: () => void;
  let releaseChildCleanup!: () => void;
  const childStarted = new Promise<void>((resolve) =>
    startChildCleanup = resolve
  );
  const childFinished = new Promise<void>((resolve) =>
    finishChildCleanup = resolve
  );
  const childCleanupGate = new Promise<void>((resolve) => {
    releaseChildCleanup = resolve;
  });
  pendingChildRoot.child("pending").register(async () => {
    startChildCleanup();
    await childCleanupGate;
    finishChildCleanup();
  });
  const childTimeoutPromise = pendingChildRoot.disposeChild("pending", 5);
  await childStarted;
  const childTimeout = await childTimeoutPromise;
  assertEquals(childTimeout.timedOut, true, "child timeout is reported");
  assertEquals(
    pendingChildRoot.childCount,
    1,
    "a timed-out pending child remains counted",
  );
  const pendingChildDrain = await pendingChildRoot.disposeAll(100);
  assertEquals(pendingChildDrain.timedOut, true, "root retains child timeout");
  releaseChildCleanup();
  await childFinished;
  await waitForCondition(
    () => pendingChildRoot.childCount === 0,
    "pending child cleanup settles",
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
        `failed enable host shuts down cleanly: ${
          JSON.stringify(report.errors)
        }`,
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
    const resultsText = results.map((result) =>
      result.status === "rejected" ? String(result.reason) : "fulfilled"
    ).join(", ");
    assert(
      results.every((result) =>
        result.status === "rejected" &&
        errorMessage(result.reason) === "M0 host closed"
      ),
      `dequeued operations reject after shutdown starts: ${resultsText}`,
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
      const host = session.host;
      const report = await session.cleanup();
      assertEquals(
        report.errors.length,
        0,
        `50-update host shuts down cleanly: ${JSON.stringify(report.errors)}`,
      );
      assertEquals(report.timedOut, false, "50-update cleanup completes");
      assertEquals(
        host.windowLifecycleDiagnosticsForTests.observers,
        false,
        "50-update shutdown removes window observers",
      );
      assertEquals(
        host.windowLifecycleDiagnosticsForTests.tracked,
        0,
        "50-update shutdown releases browser windows",
      );
      assertEquals(
        host.windowLifecycleDiagnosticsForTests.admissionTasks,
        0,
        "50-update shutdown drains admission callbacks",
      );
      assertEquals(
        host.windowLifecycleDiagnosticsForTests.readyWaiters,
        0,
        "50-update shutdown clears readiness waiters",
      );
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
    assertEquals(
      result.errors.length,
      0,
      `reopened host stops cleanly: ${JSON.stringify(result.errors)}`,
    );
    await second.cleanup();
  }
  if (first) {
    const result = await first.shutdownForRestartForTests();
    assertEquals(
      result.errors.length,
      0,
      `test host stops cleanly: ${JSON.stringify(result.errors)}`,
    );
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

  let first: M0TestHostHandle | null = null;
  let reopened: M0TestHostHandle | null = null;
  try {
    first = await createM0FeatureHostForTests(true);
    await first.host.install("rollback-pair-install", 0, "v1");
    await first.host.enable(
      "rollback-pair-enable",
      first.host.diagnostics.stateRevision,
    );
    await first.host.update(
      "rollback-pair-update-v2",
      first.host.diagnostics.stateRevision,
      "v2",
    );
    const root = first.root;
    const rollbackTxId = "rollback-pair-interrupted";
    const evaluationsBefore = m0PayloadEvaluationsForTests();
    first.failNextWriteForTests("after_replace", 0);
    const result = await first.host.rollback(
      rollbackTxId,
      first.host.diagnostics.stateRevision,
    );
    assertEquals(
      result.status,
      "storage_failed",
      "rollback write is uncertain",
    );
    const persisted = await IOUtils.readJSON(
      PathUtils.join(root, "state.json"),
    ) as JsonRecord;
    const pending = persisted.pending as JsonRecord;
    assertEquals(pending.phase, "PREPARED", "rollback PREPARED is emitted");
    assertEquals(pending.txId, rollbackTxId, "rollback keeps its original tx");
    await first.shutdownForRestartForTests();

    reopened = await createM0FeatureHostForTests(true, root, true);
    assertEquals(
      reopened.host.diagnostics.selectedVersion,
      "v2",
      "interrupted rollback restores the before selection",
    );
    assertEquals(
      reopened.host.diagnostics.rollbackVersion,
      "v1",
      "failed_restored keeps the original healthy rollback pair",
    );
    assertEquals(
      reopened.host.diagnostics.lastOutcome?.status,
      "failed_restored",
      "interrupted normal rollback is recorded as restored",
    );
    const outcome = reopened.host.queryOutcome(rollbackTxId);
    assert(
      outcome !== "outcome_unknown_or_expired",
      "interrupted rollback outcome remains retained",
    );
    assertEquals(outcome.status, "failed_restored", "rollback tx is restored");
    assertEquals(
      m0PayloadEvaluationsForTests() - evaluationsBefore,
      1,
      "rollback recovery evaluates only the before selection",
    );
  } finally {
    await finishTestHosts(first, reopened);
  }
}

async function testCommittedFallbackResumesAcrossInterruptions(): Promise<
  void
> {
  let first: M0TestHostHandle | null = null;
  let reopened: M0TestHostHandle | null = null;
  try {
    first = await createM0FeatureHostForTests(true);
    first.excludeWindowsForTests();
    await first.host.install("committed-fallback-install", 0, "v1");
    await first.host.enable(
      "committed-fallback-enable",
      first.host.diagnostics.stateRevision,
    );
    await first.host.update(
      "committed-fallback-update-v2",
      first.host.diagnostics.stateRevision,
      "v2",
    );
    const committed = first.committedPendingPublishRecordForTests();
    assert(
      committed !== null,
      "capture actual committed-pending journal write",
    );
    const committedOutcome = committed.lastOutcome as JsonRecord;
    assertEquals(
      committedOutcome.status,
      "committed_pending_publish",
      "captured state is the emitted committed-pending write",
    );
    const txId = committedOutcome.txId;
    assert(typeof txId === "string", "committed write retains its transaction");
    const root = first.root;
    const committedRevision = Number(committed.stateRevision);
    await first.shutdownForRestartForTests();
    await seedRecord(root, committed);

    const evaluationsBeforeRecovery = m0PayloadEvaluationsForTests();
    for (let interruption = 0; interruption < 2; interruption += 1) {
      let interrupted = false;
      try {
        await createM0FeatureHostForTests(true, root, true, {
          mode: "after_replace",
        });
      } catch {
        interrupted = true;
      }
      assert(
        interrupted,
        `interruption ${interruption + 1} follows the actual fallback write`,
      );
      const persisted = await IOUtils.readJSON(
        PathUtils.join(root, "state.json"),
      ) as JsonRecord;
      const pending = persisted.pending as JsonRecord;
      const attempt = persisted.attempt as JsonRecord;
      assertEquals(
        persisted.stateRevision,
        committedRevision + interruption + 1,
        "each interruption leaves one incremented emitted journal revision",
      );
      assertEquals(
        persisted.lastOutcome &&
          (persisted.lastOutcome as JsonRecord).status,
        "committed_pending_publish",
        "committed selection remains authoritative",
      );
      assertEquals(persisted.selectedVersion, "v2", "committed v2 is retained");
      assertEquals(
        persisted.desiredEnabled,
        true,
        "enabled intent is retained",
      );
      assertEquals(pending.phase, "OLD_STOPPED", "fallback is durably pending");
      assertEquals(
        pending.txId,
        txId,
        "fallback keeps the original transaction",
      );
      assertEquals(pending.targetVersion, "v1", "fallback target is persisted");
      assertEquals(
        pending.targetDigest,
        M0_FIXTURES.v1.digest,
        "persisted target has the exact v1 digest",
      );
      assertEquals(
        attempt.originTxId,
        txId,
        "attempt episode keeps original tx",
      );
      assertEquals(
        attempt.targetDigest,
        M0_FIXTURES.v1.digest,
        "attempt target matches the emitted pending target",
      );
      assertEquals(
        JSON.stringify(attempt.request),
        JSON.stringify(committedOutcome.request),
        "attempt request remains the original update descriptor",
      );
      const attemptedDigests = attempt.attemptedDigests as string[];
      assert(
        attemptedDigests.includes(M0_FIXTURES.v2.digest),
        "the committed candidate remains counted in the episode",
      );
      assert(
        !attemptedDigests.includes(M0_FIXTURES.v1.digest),
        "the interrupted fallback is still unattempted",
      );
    }
    assertEquals(
      m0PayloadEvaluationsForTests(),
      evaluationsBeforeRecovery,
      "repeated interruption before marking evaluates no fallback",
    );

    reopened = await createM0FeatureHostForTests(true, root, true);
    assertEquals(
      reopened.host.diagnostics.selectedVersion,
      "v1",
      "next open resumes the unattempted fallback",
    );
    assertEquals(
      reopened.host.diagnostics.desiredEnabled,
      true,
      "fallback recovery preserves enabled intent",
    );
    assertEquals(
      reopened.host.diagnostics.lastOutcome?.txId,
      txId,
      "fallback finalization reports the original transaction",
    );
    assertEquals(
      reopened.host.diagnostics.lastOutcome?.status,
      "failed_restored",
      "fallback finalization reports the original update as restored",
    );
    const recoveredOutcome = reopened.host.queryOutcome(txId);
    assert(
      recoveredOutcome !== "outcome_unknown_or_expired",
      "original transaction outcome remains retained",
    );
    assertEquals(
      recoveredOutcome.status,
      "failed_restored",
      "original transaction outcome is queryable after recovery",
    );
    assertEquals(reopened.host.diagnostics.pending, null, "pending is cleared");
    assertEquals(reopened.host.diagnostics.attempt, null, "episode is cleared");
    assertEquals(
      m0PayloadEvaluationsForTests() - evaluationsBeforeRecovery,
      1,
      "the unattempted v1 fallback is evaluated exactly once",
    );
    assertEquals(
      reopened.host.diagnostics.rollbackVersion,
      null,
      "unpublished v2 is not restored as v1's rollback target",
    );
    assertEquals(
      reopened.host.diagnostics.rollbackDigest,
      null,
      "unpublished v2 digest is not retained for rollback",
    );
    const evaluationsBeforeRollback = m0PayloadEvaluationsForTests();
    let rollbackError: unknown = null;
    try {
      await reopened.host.rollback(
        "committed-fallback-cannot-reselect-v2",
        reopened.host.diagnostics.stateRevision,
      );
    } catch (error) {
      rollbackError = error;
    }
    assertEquals(
      errorMessage(rollbackError),
      "missing target version",
      "rollback cannot reselect the abandoned committed v2",
    );
    assertEquals(
      reopened.host.diagnostics.selectedVersion,
      "v1",
      "rejected rollback keeps the restored v1 selection",
    );
    assertEquals(
      m0PayloadEvaluationsForTests(),
      evaluationsBeforeRollback,
      "rejected rollback does not evaluate the abandoned candidate",
    );

    assertEquals(
      (await reopened.host.update(
        "committed-rollback-update-v2",
        reopened.host.diagnostics.stateRevision,
        "v2",
      )).status,
      "succeeded",
      "adjacent rollback scenario prepares v2",
    );
    const rollbackTxId = "committed-rollback-v1";
    assertEquals(
      (await reopened.host.rollback(
        rollbackTxId,
        reopened.host.diagnostics.stateRevision,
      )).status,
      "succeeded",
      "normal rollback commits v1",
    );
    assertEquals(
      reopened.host.diagnostics.rollbackVersion,
      "v2",
      "normal rollback first retains displaced v2",
    );
    const rollbackCommitted = reopened.committedPendingPublishRecordForTests();
    assert(
      rollbackCommitted !== null,
      "capture actual committed-pending rollback write",
    );
    assertEquals(
      (rollbackCommitted.lastOutcome as JsonRecord).status,
      "committed_pending_publish",
      "adjacent recovery seed is an emitted committed rollback",
    );
    const rollbackRoot = reopened.root;
    const rollbackRecoveryEvaluations = m0PayloadEvaluationsForTests();
    await reopened.shutdownForRestartForTests();
    await seedRecord(rollbackRoot, rollbackCommitted);
    reopened = await createM0FeatureHostForTests(true, rollbackRoot, true);
    assertEquals(
      reopened.host.diagnostics.selectedVersion,
      "v2",
      "committed rollback recovery restores the prior v2 selection",
    );
    assertEquals(
      reopened.host.diagnostics.rollbackVersion,
      null,
      "unpublished rollback candidate is not retained as rollback",
    );
    assertEquals(
      reopened.host.diagnostics.lastOutcome?.txId,
      rollbackTxId,
      "adjacent recovery retains the original rollback transaction",
    );
    assertEquals(
      reopened.host.diagnostics.lastOutcome?.status,
      "failed_restored",
      "adjacent committed rollback recovery is restored",
    );
    const rollbackRecoveryOutcome = reopened.host.queryOutcome(rollbackTxId);
    assert(
      rollbackRecoveryOutcome !== "outcome_unknown_or_expired",
      "adjacent rollback transaction outcome remains retained",
    );
    assertEquals(
      rollbackRecoveryOutcome.status,
      "failed_restored",
      "adjacent rollback transaction remains queryable as restored",
    );
    assertEquals(
      m0PayloadEvaluationsForTests() - rollbackRecoveryEvaluations,
      1,
      "adjacent committed rollback evaluates one fallback candidate",
    );
    const rollbackEvaluationsAfterRecovery = m0PayloadEvaluationsForTests();
    let adjacentRollbackError: unknown = null;
    try {
      await reopened.host.rollback(
        "committed-rollback-cannot-reselect-v1",
        reopened.host.diagnostics.stateRevision,
      );
    } catch (error) {
      adjacentRollbackError = error;
    }
    assertEquals(
      errorMessage(adjacentRollbackError),
      "missing target version",
      "rollback cannot reselect the abandoned committed v1",
    );
    assertEquals(
      m0PayloadEvaluationsForTests(),
      rollbackEvaluationsAfterRecovery,
      "adjacent rejected rollback does not evaluate the abandoned candidate",
    );
  } finally {
    await finishTestHosts(first, reopened);
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

async function testNativeWindowAdmissionAndClose(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  const ownedWindows: Window[] = [];
  try {
    session = await createM0FeatureHostForTests(true);
    const host = session.host;
    await host.install("m0-window-install", 0, "v1");
    assertEquals(
      (await host.enable("m0-window-enable", host.diagnostics.stateRevision))
        .status,
      "succeeded",
      "first window enables through the parent system module",
    );
    const originalWindow = window;
    const originalId = host.windowIdForTests(originalWindow);
    assert(originalId !== null, "the runner window has a stable host ID");

    const second = openTestBrowserWindow();
    ownedWindows.push(second);
    await waitForDelayedStartup(second);
    await waitForCondition(
      () =>
        host.windowIdForTests(second) !== null &&
        host.windowIdsForTests.includes(host.windowIdForTests(second) ?? ""),
      "new normal window admission",
    );
    const secondId = host.windowIdForTests(second);
    assert(secondId !== null, "new normal window receives a stable ID");
    const marker = second.document.documentElement.getAttribute(
      "data-floorp-hotfix-m0",
    );
    assert(
      marker !== null && !marker.endsWith(":staged"),
      "new window marker is published after core attach succeeds",
    );
    const scopeCounts = host.runningScopeDiagnosticsForTests;
    assertEquals(
      scopeCounts.childScopes,
      2,
      "both browser child scopes attach",
    );
    second.close();
    await waitForCondition(
      () =>
        host.windowIdForTests(second) === null &&
        !host.windowIdsForTests.includes(secondId),
      "closed window detach",
    );
    assertEquals(
      host.runningScopeDiagnosticsForTests.childScopes,
      1,
      "close disposes only the closed window child scope",
    );
    const updated = await host.update(
      "m0-window-update-after-close",
      host.diagnostics.stateRevision,
      "v2",
    );
    assertEquals(updated.status, "succeeded", "remaining window updates");
    assert(
      host.windowIdsForTests.includes(originalId),
      "original window ID is stable across generations",
    );
    assertEquals(
      host.runningLifecycleForTests?.health(),
      "v2:1",
      "closing one window leaves the global generation active",
    );

    const popup = openTestPopupWindow();
    ownedWindows.push(popup);
    await waitForDelayedStartup(popup);
    const popupRoot = popup.document.documentElement;
    const popupWindow = popup as Window & { toolbar?: { visible?: boolean } };
    assertEquals(
      popupWindow.toolbar?.visible,
      false,
      "test popup actually hides the toolbar",
    );
    assertEquals(
      host.windowIdForTests(popup),
      null,
      `popup remains ineligible: ${
        JSON.stringify({
          windowtype: popupRoot.getAttribute("windowtype"),
          toolbarVisible: popupWindow.toolbar?.visible ?? null,
          chromehidden: popupRoot.getAttribute("chromehidden"),
          webPanelId: new URL(popup.location.href).searchParams.get(
            "floorpWebPanelId",
          ),
        })
      }`,
    );
    const privateWindow = openTestBrowserWindow("width=640,height=480", true);
    ownedWindows.push(privateWindow);
    await waitForDelayedStartup(privateWindow);
    const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
    );
    assert(
      PrivateBrowsingUtils.isWindowPrivate(privateWindow),
      "test window is actually private",
    );
    assertEquals(
      host.windowIdForTests(privateWindow),
      null,
      "private browser window is never admitted",
    );
  } finally {
    for (const owned of ownedWindows) if (!owned.closed) owned.close();
    if (session) {
      const host = session.host;
      await session.cleanup();
      assertEquals(
        host.windowLifecycleDiagnosticsForTests.observers,
        false,
        "shutdown removes host window observers",
      );
      assertEquals(
        host.windowLifecycleDiagnosticsForTests.tracked,
        0,
        "shutdown releases tracked window references",
      );
      assertEquals(
        host.windowLifecycleDiagnosticsForTests.pendingStartup,
        0,
        "shutdown removes load callbacks",
      );
    }
  }
}

async function testLateWindowAttachCannotDeleteReplacement(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  let gate:
    | ReturnType<
      M0TestHostHandle["deferWindowAttachCompletionForTests"]
    >
    | null = null;
  const ownedWindows: Window[] = [];
  try {
    session = await createM0FeatureHostForTests(true);
    const host = session.host;
    await host.install("m0-late-window-install", 0, "v1");
    await host.enable("m0-late-window-enable", host.diagnostics.stateRevision);
    await waitForCondition(
      () => host.windowLifecycleDiagnosticsForTests.admissionTasks === 0,
      "initial admissions settle",
    );

    gate = session.deferWindowAttachCompletionForTests();
    const later = openTestBrowserWindow();
    ownedWindows.push(later);
    await waitForDelayedStartup(later);
    const laterId = host.windowIdForTests(later);
    assert(laterId !== null, "late window has stable ID");
    await within(gate.entered, 15_000, "old generation attach completion");

    const updated = await within(
      host.update(
        "m0-late-window-update",
        host.diagnostics.stateRevision,
        "v2",
      ),
      30_000,
      "replacement generation update",
    );
    assertEquals(updated.status, "succeeded", "replacement generation commits");
    assert(
      host.windowIdsForTests.includes(laterId),
      "replacement generation owns the admitted window before old attach settles",
    );
    assertEquals(
      host.runningScopeDiagnosticsForTests.childScopes,
      2,
      "replacement has both window scopes",
    );

    gate.release();
    await waitForCondition(
      () => host.windowLifecycleDiagnosticsForTests.admissionTasks === 0,
      "old admission settles",
    );
    assert(
      host.windowIdsForTests.includes(laterId),
      "stale attach completion cannot delete the replacement map entry",
    );
    assertEquals(
      host.runningScopeDiagnosticsForTests.childScopes,
      2,
      "stale attach completion leaves replacement scopes intact",
    );
  } finally {
    gate?.release();
    for (const owned of ownedWindows) if (!owned.closed) owned.close();
    if (session) await session.cleanup();
  }
}

async function testNativeWindowCloseDuringGatedUpdate(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  let gate: ReturnType<M0TestHostHandle["deferWindowAttachForTests"]> | null =
    null;
  const ownedWindows: Window[] = [];
  try {
    session = await createM0FeatureHostForTests(true);
    const host = session.host;
    await host.install("m0-window-gate-install", 0, "v1");
    await host.enable("m0-window-gate-enable", host.diagnostics.stateRevision);
    const gatedWindow = openTestBrowserWindow();
    ownedWindows.push(gatedWindow);
    await waitForDelayedStartup(gatedWindow);
    await waitForCondition(
      () =>
        host.windowIdForTests(gatedWindow) !== null &&
        host.windowIdsForTests.includes(
          host.windowIdForTests(gatedWindow) ?? "",
        ),
      "gated window initial admission",
    );
    const gatedId = host.windowIdForTests(gatedWindow);
    assert(gatedId !== null, "gated window has stable ID");
    gate = session.deferWindowAttachForTests(gatedId);
    const update = host.update(
      "m0-window-gated-update",
      host.diagnostics.stateRevision,
      "v2",
    );
    await within(gate.entered, 15_000, "gated W0 attachment");

    const during = openTestBrowserWindow();
    ownedWindows.push(during);
    await waitForDelayedStartup(during);
    const duringId = host.windowIdForTests(during);
    assert(duringId !== null, "during-operation window is tracked");
    assert(
      !host.windowIdsForTests.includes(duringId),
      "new window is not added to finite W0 mid-operation",
    );
    gatedWindow.close();
    const result = await within(update, 15_000, "update after W0 close");
    assertEquals(
      result.status,
      "succeeded",
      "closing W0 member releases attach",
    );
    gate.release();
    await waitForCondition(
      () => host.windowIdsForTests.includes(duringId),
      "post-commit admission of during-operation window",
    );
    assert(
      !host.windowIdsForTests.includes(gatedId),
      "closed W0 member is detached",
    );
  } finally {
    gate?.release();
    for (const owned of ownedWindows) if (!owned.closed) owned.close();
    if (session) await session.cleanup();
  }
}

async function testParentModuleSurvivesOwnedInitialWindowClose(): Promise<
  void
> {
  let session: M0TestHostHandle | null = null;
  let initial: Window | null = null;
  try {
    session = await createM0FeatureHostForTests(true);
    const host = session.host;
    initial = openTestBrowserWindow();
    await waitForDelayedStartup(initial);
    session.useOnlyWindowForTests(initial);
    await host.install("m0-owned-initial-install", 0, "v1");
    assertEquals(
      (await host.enable(
        "m0-owned-initial-enable",
        host.diagnostics.stateRevision,
      )).status,
      "succeeded",
      "parent system ESM host starts on a test-owned initial window",
    );
    const initialId = host.windowIdForTests(initial);
    assert(initialId !== null, "test-owned initial window has a host ID");
    initial.close();
    await waitForCondition(
      () => !host.windowIdsForTests.includes(initialId),
      "test-owned initial window detach",
    );
    assertEquals(
      host.runningLifecycleForTests?.health(),
      "v1:1",
      "closing the first attached window leaves global generation alive",
    );
    const update = await host.update(
      "m0-owned-initial-update",
      host.diagnostics.stateRevision,
      "v2",
    );
    assertEquals(
      update.status,
      "succeeded",
      "host remains operable after close",
    );
    assertEquals(
      host.runningLifecycleForTests?.health(),
      "v2:1",
      "surviving parent host attaches to remaining normal windows",
    );
  } finally {
    if (initial && !initial.closed) initial.close();
    if (session) await session.cleanup();
  }
}

async function testPostcommitWindowFailureQuarantines(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  let extra: Window | null = null;
  try {
    session = await createM0FeatureHostForTests(true);
    const host = session.host;
    await host.install("m0-window-failure-install", 0, "v1");
    await host.enable(
      "m0-window-failure-enable",
      host.diagnostics.stateRevision,
    );
    const previous = host.runningLifecycleForTests;
    assert(previous !== null, "published generation is active");
    session.failWindowAttachForTests("*");
    extra = openTestBrowserWindow();
    await waitForDelayedStartup(extra);
    await waitForCondition(
      () =>
        host.diagnostics.blockedReason !== null &&
        host.runningLifecycleForTests === null,
      "post-commit attach quarantine",
    );
    assertEquals(
      host.diagnostics.desiredEnabled,
      true,
      "post-commit failure preserves desired intent",
    );
    assertEquals(
      host.diagnostics.lastOutcome?.status,
      "succeeded",
      "completed terminal outcome is not overwritten",
    );
    assertThrows(() => previous.health(), "failed generation is stopped");
    assertEquals(
      host.runningLifecycleForTests,
      null,
      "old generation is not revived after admission failure",
    );
  } finally {
    if (extra && !extra.closed) extra.close();
    if (session) await session.cleanup();
  }
}

async function testEmptyWindowSetAndSandboxCleanupFailures(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  try {
    session = await createM0FeatureHostForTests(true);
    session.excludeWindowsForTests();
    await session.host.install("m0-empty-w0-install", 0, "v1");
    const enabled = await session.host.enable(
      "m0-empty-w0-enable",
      session.host.diagnostics.stateRevision,
    );
    assertEquals(enabled.status, "succeeded", "empty W0 activates fixture");
    assertEquals(
      session.host.windowIdsForTests.length,
      0,
      "empty W0 stays empty",
    );
    assertEquals(
      session.host.runningLifecycleForTests?.health(),
      "v1:1",
      "empty W0 still runs the fixture lifecycle",
    );
  } finally {
    if (session) await session.cleanup();
  }

  const makeRuntime = (deactivate: string) =>
    new M0ModuleHost(
      "cleanup-test-epoch",
      1,
      "e".repeat(64),
      "globalThis.__floorpM0 = { activate() {}, attachWindow() {}, " +
        'health() { return "ok"; }, deactivate() { ' + deactivate + " } };",
      () => "{}",
    );
  for (
    const [name, deactivate] of [
      ["throws", 'throw new Error("deactivate failure");'],
      ["times out", "return new Promise(() => {});"],
    ] as const
  ) {
    const runtime = makeRuntime(deactivate);
    let disposed = false;
    runtime.scope.register(() => {
      disposed = true;
    });
    await runtime.start();
    const report = await runtime.stop(name);
    assert(disposed, `host disposer runs when deactivate ${name}`);
    assert(
      report.errors.length > 0,
      `deactivate ${name} is reported to the host`,
    );
    if (name === "times out") {
      assert(
        report.errors.some((error) => error.includes("lifecycle deadline")),
        "async deactivate timeout is reported",
      );
    }
  }

  let activeGeneration = 1;
  let currentResourceMutations = 0;
  const staleSource =
    'globalThis.__floorpM0 = { activate(_context, request) { this.retainedRequest = request; }, attachWindow() {}, health() { return "ok"; }, deactivate() {} };';
  const staleRuntime = new M0ModuleHost(
    "stale-callback-epoch",
    1,
    "f".repeat(64),
    staleSource,
    (_epoch, generation) => {
      if (generation !== activeGeneration) throw new Error("stale token");
      currentResourceMutations += 1;
      return "{}";
    },
  );
  await staleRuntime.start();
  const lifecycle = staleRuntime.lifecycleForTests as M0Lifecycle & {
    retainedRequest?: (json: string) => string;
  };
  const retainedRequest = lifecycle.retainedRequest;
  assert(retainedRequest, "fixture retained its broker callback");
  await staleRuntime.stop("replace");
  activeGeneration = 2;
  try {
    retainedRequest(JSON.stringify({ type: "marker", windowId: "old" }));
  } catch (error) {
    assert(error !== undefined, "stale broker callback rejects after stop");
  }
  assertEquals(
    currentResourceMutations,
    0,
    "old retained broker callback cannot mutate the current token resource",
  );
}

async function testStagedWindowCloseCleanupFailureQuarantines(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  let ownedWindow: Window | null = null;
  let gate:
    | ReturnType<
      M0TestHostHandle["deferWindowAttachCompletionForTests"]
    >
    | null = null;
  try {
    ownedWindow = openTestBrowserWindow();
    await waitForDelayedStartup(ownedWindow);
    session = await createM0FeatureHostForTests(true);
    session.useOnlyWindowForTests(ownedWindow);
    const host = session.host;
    await host.install("m0-staged-close-install", 0, "v1");
    const windowId = host.windowIdForTests(ownedWindow);
    assert(windowId !== null, "staged test window has a stable ID");
    gate = session.deferWindowAttachCompletionForTests(windowId);
    const enabling = host.enable(
      "m0-staged-close-enable",
      host.diagnostics.stateRevision,
    );
    await within(gate.entered, 15_000, "staged window core attach");
    session.registerWindowCleanupForTests(windowId, () => {
      throw new Error("staged child cleanup failure");
    });
    ownedWindow.close();
    await waitForCondition(
      () => host.windowLifecycleDiagnosticsForTests.cleanupFailures > 0,
      "staged child cleanup failure is observed",
    );
    gate.release();
    const outcome = await within(enabling, 30_000, "staged failure quarantine");
    assertEquals(
      outcome.status,
      "failed_quarantined",
      "staged child cleanup failure prevents candidate commit",
    );
    assertEquals(
      host.diagnostics.blockedReason,
      "candidate-window-cleanup-failure",
      "staged cleanup failure is durably diagnosed",
    );
    assertEquals(
      host.runningLifecycleForTests,
      null,
      "staged candidate does not remain active after cleanup failure",
    );
    assertEquals(
      host.diagnostics.pending,
      null,
      "quarantine clears the unfinished pending operation",
    );
  } finally {
    gate?.release();
    if (ownedWindow && !ownedWindow.closed) ownedWindow.close();
    if (session) await session.cleanup();
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
      name: "actual journal phases and rollback pair recovery",
      fn: testActualPendingPhaseRecovery,
    },
    {
      name: "committed fallback resumes after repeated pre-mark interruption",
      fn: testCommittedFallbackResumesAcrossInterruptions,
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
      name: "disabled update to rollback target preserves distinct version",
      fn: testDisabledUpdateToRollbackKeepsDistinctTarget,
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
    {
      name: "native main-window admission and close detach",
      fn: testNativeWindowAdmissionAndClose,
    },
    {
      name: "late old-window admission cannot delete replacement state",
      fn: testLateWindowAttachCannotDeleteReplacement,
    },
    {
      name: "native W0 close cancels gated attach without starving update",
      fn: testNativeWindowCloseDuringGatedUpdate,
    },
    {
      name: "parent system module survives first attached window close",
      fn: testParentModuleSurvivesOwnedInitialWindowClose,
    },
    {
      name: "post-commit window attach failure quarantines generation",
      fn: testPostcommitWindowFailureQuarantines,
    },
    {
      name: "staged window close cleanup failure quarantines candidate",
      fn: testStagedWindowCloseCleanupFailureQuarantines,
    },
    {
      name: "empty W0 and sandbox cleanup failures",
      fn: testEmptyWindowSetAndSandboxCleanupFailures,
    },
  ];
  await runTests("M0FeatureHost.test.mts", tests);
}
