// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  createM0FeatureHostForTests,
  type M0TestHostHandle,
} from "./FeatureHost.sys.mts";
import { M0_FIXTURES } from "./M0Fixtures.sys.mts";
import { M0ModuleHost, ResourceScope } from "./M0ModuleHost.sys.mts";

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

    const enabled = await host.enable(
      "m0-enable-1",
      host.diagnostics.stateRevision,
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
      "broken fixture is rejected",
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
    "shared stop report includes no cleanup errors",
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
