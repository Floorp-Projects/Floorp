// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert as harnessAssert,
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import type { FeatureDefinition, PinnedFixture } from "./FeatureHostTypes.ts";
import type { HubSnapshotEntry } from "./HubRegistryTypes.ts";

type FeatureHostModule = typeof import("./FeatureHost.sys.mts");
const {
  createFeatureHostForTests,
  createM0FeatureHostForTests,
  M0FeatureHost,
} = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs",
) as FeatureHostModule;
type HubRegistryModule = typeof import("./HubRegistry.sys.mts");
const { HubRegistry } = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/HubRegistry.sys.mjs",
) as HubRegistryModule;

const BASELINE_M0_V1_DIGEST =
  "614d7220b1730a52e7864a4d30263f8fb9b96e7b42143394cb21fd04b5d9d07f";

function assert(condition: boolean, message = "assertion failed"): void {
  harnessAssert(condition, message);
}

function assertEquals<T>(
  actual: T,
  expected: T,
  message = "values should match",
): void {
  harnessAssertEquals(actual, expected, message);
}

interface MutablePinnedFixture {
  version: string;
  digest: string;
  source: string;
  compatibility: {
    hostSchema: number;
    hostBuildId: string;
    runtimeBuildIds: string[];
    capabilities: string[];
  };
}

interface MutableFeatureDefinition {
  featureId: string;
  fixtures: Record<string, MutablePinnedFixture>;
}

function sourceFor(featureId: string, version: string): string {
  return `(() => { globalThis.__floorpM0 = { activate(context, request) { request(JSON.stringify({ type: "hub.register", descriptor: { localId: "sample", route: "/extensions/${featureId}/sample", titleKey: "hotfixHub.sample.title", navigation: { labelKey: "hotfixHub.sample.navigation", order: 10 }, view: { kind: "message", titleKey: "hotfixHub.sample.title", bodyKey: "hotfixHub.sample.body", params: { feature: "${featureId}", version: "${version}" } } } })); }, attachWindow() {}, health() { return "${featureId}:${version}"; }, deactivate() {} }; })();`;
}

async function pinnedFixture(
  featureId: string,
  version: string,
): Promise<PinnedFixture<string>> {
  const source = sourceFor(featureId, version);
  const bytes = new TextEncoder().encode(source);
  const data = new ArrayBuffer(bytes.byteLength);
  new Uint8Array(data).set(bytes);
  const result = await crypto.subtle.digest("SHA-256", data);
  const digest = [...new Uint8Array(result)].map((byte) =>
    byte.toString(16).padStart(2, "0")
  ).join("");
  return {
    version,
    digest,
    source,
    compatibility: {
      hostSchema: 2,
      hostBuildId: "m0-schema-2",
      runtimeBuildIds: [Services.appinfo.appBuildID],
      capabilities: ["hub-pages"],
    },
  };
}

async function definitionFor(featureId: string): Promise<FeatureDefinition> {
  return await definitionForVersions(featureId, ["v1", "v2"]);
}

async function definitionForVersions(
  featureId: string,
  versions: readonly string[],
): Promise<FeatureDefinition> {
  const fixtures: Record<string, PinnedFixture<string>> = Object.create(null);
  for (const version of versions) {
    fixtures[version] = await pinnedFixture(featureId, version);
  }
  return { featureId, fixtures };
}

async function mutableDefinitionFor(
  featureId: string,
): Promise<MutableFeatureDefinition> {
  const definition = await definitionFor(featureId);
  const fixtures: Record<string, MutablePinnedFixture> = {};
  for (const [version, fixture] of Object.entries(definition.fixtures)) {
    fixtures[version] = {
      version: fixture.version,
      digest: fixture.digest,
      source: fixture.source,
      compatibility: {
        hostSchema: fixture.compatibility.hostSchema,
        hostBuildId: fixture.compatibility.hostBuildId,
        runtimeBuildIds: [...fixture.compatibility.runtimeBuildIds],
        capabilities: [...fixture.compatibility.capabilities],
      },
    };
  }
  return { featureId, fixtures };
}

async function rejectionOf(promise: Promise<unknown>): Promise<unknown> {
  try {
    await promise;
    return null;
  } catch (error) {
    return error;
  }
}

function messageOf(error: unknown): string {
  return typeof error === "object" && error !== null && "message" in error
    ? String(error.message)
    : String(error);
}

function featureEntries(
  registry: InstanceType<typeof HubRegistry>,
  featureId: string,
): readonly HubSnapshotEntry[] {
  return registry.getSnapshotForTests().entries.filter((entry) =>
    entry.featureId === featureId
  );
}

function assertCleanDisposeReport(
  report: { errors: string[]; timedOut: boolean; pendingTasks?: number },
  name: string,
): void {
  assertEquals(report.errors.length, 0, `${name} cleanup reports no errors`);
  assertEquals(report.timedOut, false, `${name} cleanup does not time out`);
  assertEquals(
    report.pendingTasks ?? 0,
    0,
    `${name} cleanup has no pending tasks`,
  );
}

function assertRegistryClear(
  registry: InstanceType<typeof HubRegistry>,
  name: string,
): void {
  const diagnostics = registry.diagnosticsForTests;
  assertEquals(diagnostics.claims, 0, `${name} has no registry claims`);
  assertEquals(diagnostics.leases, 0, `${name} has no registry leases`);
  assertEquals(
    diagnostics.pendingAcknowledgements,
    0,
    `${name} has no pending acknowledgements`,
  );
  assertEquals(diagnostics.pendingTimers, 0, `${name} has no pending timers`);
  assertEquals(diagnostics.stagedEntries, 0, `${name} has no staged entries`);
  assertEquals(
    diagnostics.publishedEntries,
    0,
    `${name} has no published entries`,
  );
  assertEquals(diagnostics.clients, 0, `${name} has no clients`);
}

interface CleanupAttempt {
  name: string;
  result: PromiseSettledResult<
    {
      errors: string[];
      timedOut: boolean;
      pendingTasks?: number;
    } | void
  >;
}

async function collectCleanupAttempt(
  name: string,
  cleanup: () => Promise<
    {
      errors: string[];
      timedOut: boolean;
      pendingTasks?: number;
    } | void
  >,
): Promise<CleanupAttempt> {
  try {
    return {
      name,
      result: { status: "fulfilled", value: await cleanup() },
    };
  } catch (reason) {
    return { name, result: { status: "rejected", reason } };
  }
}

function assertCleanupAttempts(
  attempts: readonly CleanupAttempt[],
  checks: readonly { name: string; check: () => void }[],
): void {
  const failures: string[] = [];
  for (const { name, result } of attempts) {
    if (result.status === "rejected") {
      failures.push(`${name} cleanup rejected: ${messageOf(result.reason)}`);
    } else if (result.value !== undefined) {
      try {
        assertCleanDisposeReport(result.value, name);
      } catch (error) {
        failures.push(messageOf(error));
      }
    }
  }
  for (const { name, check } of checks) {
    try {
      check();
    } catch (error) {
      failures.push(`${name}: ${messageOf(error)}`);
    }
  }
  if (failures.length > 0) throw new Error(failures.join("; "));
}

async function testIndependentFeatureLifecycle(): Promise<void> {
  const featureA = "m2-feature-alpha";
  const featureB = "m2-feature-beta";
  const definitionA = await definitionForVersions(featureA, [
    "v1",
    "v2",
    "1.2.0",
  ]);
  const definitionB = await definitionFor(featureB);
  assertEquals(
    definitionA.fixtures.v1.version,
    definitionB.fixtures.v1.version,
  );
  assert(
    definitionA.fixtures.v1.digest !== definitionB.fixtures.v1.digest,
    "overlapping labels must use distinct pinned sources",
  );
  const registry = new HubRegistry({ epoch: "m2-feature-isolation" });
  const testOptions = {
    excludeWindowsForTests: true,
    hubRegistryForTests: registry,
  };
  let hostA: Awaited<ReturnType<typeof createFeatureHostForTests>> | null =
    null;
  let reopenedA: Awaited<ReturnType<typeof createFeatureHostForTests>> | null =
    null;
  let hostB: Awaited<ReturnType<typeof createFeatureHostForTests>> | null =
    null;
  try {
    hostA = await createFeatureHostForTests(
      definitionA,
      true,
      undefined,
      false,
      testOptions,
    );
    hostB = await createFeatureHostForTests(
      definitionB,
      true,
      undefined,
      false,
      testOptions,
    );
    assertEquals(
      (await hostB.host.install(
        "install",
        hostB.host.diagnostics.stateRevision,
        "v1",
      )).status,
      "succeeded",
    );
    assertEquals(
      (await hostB.host.enable(
        "enable",
        hostB.host.diagnostics.stateRevision,
      )).status,
      "succeeded",
    );
    const initialBRevision = hostB.host.diagnostics.stateRevision;
    const initialBGeneration = hostB.host.generationForTests;
    const initialBDesiredEnabled = hostB.host.diagnostics.desiredEnabled;
    const initialBEntries = JSON.stringify(featureEntries(registry, featureB));
    assertEquals(
      initialBEntries.includes(`/extensions/${featureB}/sample`),
      true,
    );
    assertEquals(
      initialBEntries.includes(`/extensions/${featureA}/sample`),
      false,
    );

    const assertBUnchanged = () => {
      assertEquals(hostB!.host.diagnostics.stateRevision, initialBRevision);
      assertEquals(hostB!.host.generationForTests, initialBGeneration);
      assertEquals(
        hostB!.host.diagnostics.desiredEnabled,
        initialBDesiredEnabled,
      );
      assertEquals(
        JSON.stringify(featureEntries(registry, featureB)),
        initialBEntries,
      );
    };

    assertEquals(
      (await hostA.host.install(
        "install",
        hostA.host.diagnostics.stateRevision,
        "v1",
      )).status,
      "succeeded",
    );
    assertBUnchanged();
    assertEquals(
      (await hostA.host.enable(
        "enable",
        hostA.host.diagnostics.stateRevision,
      )).status,
      "succeeded",
    );
    assertBUnchanged();
    const absentVersion = await rejectionOf(
      hostB.host.update(
        "shared-update",
        hostB.host.diagnostics.stateRevision,
        "1.2.0",
      ),
    );
    assert(
      messageOf(absentVersion).includes("invalid transaction descriptor"),
      "feature B rejects a version absent from its own catalog",
    );
    assertBUnchanged();
    assertEquals(
      (await hostA.host.update(
        "shared-update",
        hostA.host.diagnostics.stateRevision,
        "1.2.0",
      )).status,
      "succeeded",
    );
    assertBUnchanged();
    assertEquals(
      (await hostA.host.disable(
        "a-disable",
        hostA.host.diagnostics.stateRevision,
      )).status,
      "succeeded",
    );
    assertBUnchanged();
    assertEquals(featureEntries(registry, featureA).length, 0);
    const rootA = hostA.root;
    assertCleanDisposeReport(
      await hostA.shutdownForRestartForTests(),
      "A restart shutdown",
    );
    reopenedA = await createFeatureHostForTests(
      definitionA,
      true,
      rootA,
      true,
      testOptions,
    );
    assertEquals(reopenedA.host.diagnostics.featureId, featureA);
    assertEquals(reopenedA.host.diagnostics.selectedVersion, "1.2.0");
    assertEquals(reopenedA.host.diagnostics.desiredEnabled, false);
    assertBUnchanged();
  } finally {
    const cleanupAttempts: CleanupAttempt[] = [];
    if (reopenedA) {
      cleanupAttempts.push(
        await collectCleanupAttempt(
          "reopened A",
          () => reopenedA!.cleanup(),
        ),
      );
    }
    if (hostA) {
      cleanupAttempts.push(
        await collectCleanupAttempt("A", () => hostA!.cleanup()),
      );
    }
    if (hostB) {
      cleanupAttempts.push(
        await collectCleanupAttempt("B", () => hostB!.cleanup()),
      );
    }
    assertCleanupAttempts(cleanupAttempts, [{
      name: "feature isolation registry",
      check: () => assertRegistryClear(registry, "feature isolation registry"),
    }]);
  }
}

async function testCatalogValidationAndCallerMutation(): Promise<void> {
  const featureId = "m2-feature-mutable";
  const registry = new HubRegistry({ epoch: "m2-feature-mutation" });
  const input = await mutableDefinitionFor(featureId);
  const expectedSource = input.fixtures.v1.source;
  const expectedDigest = input.fixtures.v1.digest;
  const host = await createFeatureHostForTests(input, true, undefined, false, {
    excludeWindowsForTests: true,
    hubRegistryForTests: registry,
  });
  try {
    input.fixtures.v1.source = "throw new Error('caller mutation');";
    input.fixtures.v1.digest = "0".repeat(64);
    input.fixtures.v1.compatibility.runtimeBuildIds[0] = "wrong-runtime";
    input.fixtures.v1.compatibility.capabilities[0] = "window-marker";
    assertEquals(
      (await host.host.install(
        "mutable-install",
        host.host.diagnostics.stateRevision,
        "v1",
      )).status,
      "succeeded",
    );
    assertEquals(
      (await host.host.enable(
        "mutable-enable",
        host.host.diagnostics.stateRevision,
      )).status,
      "succeeded",
    );
    assertEquals(host.host.diagnostics.selectedDigest, expectedDigest);
    const entries = featureEntries(registry, featureId);
    assertEquals(entries.length, 1);
    assertEquals(entries[0].route, `/extensions/${featureId}/sample`);
    assert(expectedSource.includes(featureId));

    const revisionBeforeWrongVersion = host.host.diagnostics.stateRevision;
    const invalidVersion = await rejectionOf(
      host.host.install(
        "wrong-version",
        revisionBeforeWrongVersion,
        "hub-v1",
      ),
    );
    assert(
      messageOf(invalidVersion).includes("invalid transaction descriptor"),
      "a version absent from the selected catalog must be rejected",
    );
    assertEquals(
      host.host.diagnostics.stateRevision,
      revisionBeforeWrongVersion,
    );
  } finally {
    const cleanupAttempts = [
      await collectCleanupAttempt("mutable catalog", () => host.cleanup()),
    ];
    assertCleanupAttempts(cleanupAttempts, [{
      name: "mutation registry",
      check: () => assertRegistryClear(registry, "mutation registry"),
    }]);
  }

  const badDigestDefinition = await mutableDefinitionFor(
    "m2-feature-bad-digest",
  );
  badDigestDefinition.fixtures.v1.digest = "0".repeat(64);
  const badDigestRegistry = new HubRegistry({ epoch: "m2-feature-bad-digest" });
  const badDigestHost = await createFeatureHostForTests(
    badDigestDefinition,
    true,
    undefined,
    false,
    {
      excludeWindowsForTests: true,
      hubRegistryForTests: badDigestRegistry,
    },
  );
  try {
    const failure = await rejectionOf(
      badDigestHost.host.install(
        "bad-digest",
        badDigestHost.host.diagnostics.stateRevision,
        "v1",
      ),
    );
    assert(
      messageOf(failure).includes("pinned fixture digest mismatch"),
      "source must match the selected catalog digest",
    );
    assertEquals(
      badDigestHost.host.diagnostics.stateRevision,
      0,
      "failed digest validation must not change the record",
    );
  } finally {
    const cleanupAttempts = [
      await collectCleanupAttempt("bad digest", () => badDigestHost.cleanup()),
    ];
    assertCleanupAttempts(cleanupAttempts, [{
      name: "bad digest registry",
      check: () =>
        assertRegistryClear(badDigestRegistry, "bad digest registry"),
    }]);
  }

  const markerDefinition = await mutableDefinitionFor("m2-feature-marker");
  markerDefinition.fixtures.v1.compatibility.capabilities = ["window-marker"];
  const markerFailure = await rejectionOf(
    createFeatureHostForTests(markerDefinition, true),
  );
  assert(
    messageOf(markerFailure).includes("invalid feature compatibility metadata"),
    "non-M0 definitions cannot claim the shared window marker",
  );
}

async function testWrongRecordIdentityDigestAndRootConflict(): Promise<void> {
  const featureA = "m2-feature-record-a";
  const featureB = "m2-feature-record-b";
  const definitionA = await definitionFor(featureA);
  const definitionB = await definitionFor(featureB);
  const registry = new HubRegistry({ epoch: "m2-feature-records" });
  const testOptions = {
    excludeWindowsForTests: true,
    hubRegistryForTests: registry,
  };
  let host: Awaited<ReturnType<typeof createFeatureHostForTests>> | null = null;
  type OpenedFeatureHost = Awaited<
    ReturnType<typeof M0FeatureHost.openForTests>
  >;
  let openingHost: OpenedFeatureHost | null = null;
  let openingHostPromise: Promise<OpenedFeatureHost> | null = null;
  let openingConflictPromise: Promise<unknown> | null = null;
  let openingRoot: string | null = null;
  const openingRegistry = new HubRegistry({ epoch: "m2-feature-opening" });
  const openingOptions = {
    excludeWindowsForTests: true,
    hubRegistryForTests: openingRegistry,
  };
  try {
    openingRoot = PathUtils.join(
      PathUtils.profileDir,
      "floorp-hotfix-m0-tests",
      `feature-${Services.uuid.generateUUID().toString().replace(/[{}]/g, "")}`,
    );
    openingHostPromise = M0FeatureHost.openForTests(
      openingRoot,
      true,
      false,
      undefined,
      openingOptions,
      definitionA,
    );
    openingConflictPromise = rejectionOf(
      M0FeatureHost.openForTests(
        openingRoot,
        true,
        false,
        undefined,
        openingOptions,
        definitionB,
      ),
    );
    openingHost = await openingHostPromise;
    const openingConflict = await openingConflictPromise;
    assert(
      messageOf(openingConflict).includes("feature definition conflicts"),
      "a conflicting definition is rejected while the root is opening",
    );
    host = await createFeatureHostForTests(
      definitionA,
      true,
      undefined,
      false,
      testOptions,
    );
    const inconsistentCatalog = await mutableDefinitionFor(featureA);
    inconsistentCatalog.fixtures.v1.source += " ";
    const catalogConflict = await rejectionOf(
      createFeatureHostForTests(
        inconsistentCatalog,
        true,
        host.root,
        false,
        testOptions,
      ),
    );
    assert(
      messageOf(catalogConflict).includes("feature definition conflicts"),
      "a shared root cannot be reopened with a different pinned catalog",
    );
    const conflict = await rejectionOf(
      createFeatureHostForTests(
        definitionB,
        true,
        host.root,
        false,
        testOptions,
      ),
    );
    assert(
      messageOf(conflict).includes("feature definition conflicts"),
      "a shared root cannot return a host with a different definition",
    );
    assertEquals(
      (await host.host.install(
        "record-install",
        host.host.diagnostics.stateRevision,
        "v1",
      )).status,
      "succeeded",
    );
    const root = host.root;
    assertCleanDisposeReport(
      await host.shutdownForRestartForTests(),
      "record restart shutdown",
    );

    const recordPath = PathUtils.join(root, "state.json");
    const record = await IOUtils.readJSON(recordPath) as Record<
      string,
      unknown
    >;
    record.featureId = featureB;
    await IOUtils.writeJSON(recordPath, record);
    const identityFailure = await rejectionOf(
      createFeatureHostForTests(definitionA, true, root, true, testOptions),
    );
    assert(messageOf(identityFailure).includes("invalid M0 state record"));

    record.featureId = featureA;
    record.selectedDigest = definitionA.fixtures.v2.digest;
    await IOUtils.writeJSON(recordPath, record);
    const digestFailure = await rejectionOf(
      createFeatureHostForTests(definitionA, true, root, true, testOptions),
    );
    assert(messageOf(digestFailure).includes("invalid M0 state record"));
  } finally {
    if (openingHostPromise) {
      const [result] = await Promise.allSettled([openingHostPromise]);
      if (result.status === "fulfilled") openingHost ??= result.value;
    }
    if (openingConflictPromise) await openingConflictPromise;
    const cleanupAttempts: CleanupAttempt[] = [];
    if (host) {
      cleanupAttempts.push(
        await collectCleanupAttempt(
          "record host",
          () => host!.cleanup(),
        ),
      );
    }
    if (openingHost) {
      cleanupAttempts.push(
        await collectCleanupAttempt(
          "opening-root host",
          () => openingHost!.shutdown(),
        ),
      );
    }
    if (openingRoot) {
      cleanupAttempts.push(
        await collectCleanupAttempt(
          "opening-root directory",
          () =>
            IOUtils.remove(openingRoot!, {
              recursive: true,
              ignoreAbsent: true,
            }),
        ),
      );
    }
    assertCleanupAttempts(cleanupAttempts, [
      {
        name: "record registry",
        check: () => assertRegistryClear(registry, "record registry"),
      },
      {
        name: "opening-root registry",
        check: () =>
          assertRegistryClear(openingRegistry, "opening-root registry"),
      },
    ]);
  }
}

async function testM0RecordReopensWithoutSchemaChange(): Promise<void> {
  const root = PathUtils.join(
    PathUtils.profileDir,
    "floorp-hotfix-m0-tests",
    `m0-${Services.uuid.generateUUID().toString().replace(/[{}]/g, "")}`,
  );
  await IOUtils.makeDirectory(root, { createAncestors: true });
  await IOUtils.writeJSON(PathUtils.join(root, "state.json"), {
    schemaVersion: 2,
    featureId: "experimental-local-sample",
    desiredEnabled: false,
    selectedVersion: "v1",
    selectedDigest: BASELINE_M0_V1_DIGEST,
    selectedHealth: "unverified",
    selectedHealthBuild: null,
    rollbackVersion: null,
    rollbackDigest: null,
    rollbackHealthBuild: null,
    stateRevision: 7,
    pending: null,
    attempt: null,
    blockedReason: null,
    recentOutcomes: [],
    lastOutcome: {
      txId: "legacy-install-v1",
      request: {
        featureId: "experimental-local-sample",
        op: "install",
        expectedRevision: 6,
        target: "v1",
        recoveryEpoch: null,
        normalizedInput: "install:v1",
      },
      status: "succeeded",
      selectedVersion: "v1",
    },
  });
  const registry = new HubRegistry({ epoch: "m2-m0-legacy-record" });
  let reopened: Awaited<ReturnType<typeof createM0FeatureHostForTests>> | null =
    null;
  try {
    reopened = await createM0FeatureHostForTests(true, root, true, undefined, {
      excludeWindowsForTests: true,
      hubRegistryForTests: registry,
    });
    assertEquals(
      reopened.host.diagnostics.featureId,
      "experimental-local-sample",
    );
    assertEquals(reopened.host.diagnostics.schemaVersion, 2);
    assertEquals(reopened.host.diagnostics.selectedVersion, "v1");
    assertEquals(
      reopened.host.diagnostics.selectedDigest,
      BASELINE_M0_V1_DIGEST,
    );
    assertEquals(reopened.host.diagnostics.stateRevision, 7);
    assertEquals(reopened.host.diagnostics.desiredEnabled, false);
  } finally {
    const cleanupAttempts: CleanupAttempt[] = [];
    if (reopened) {
      cleanupAttempts.push(
        await collectCleanupAttempt(
          "legacy M0 record",
          () => reopened!.cleanup(),
        ),
      );
    }
    cleanupAttempts.push(
      await collectCleanupAttempt(
        "legacy M0 record directory",
        () => IOUtils.remove(root, { recursive: true, ignoreAbsent: true }),
      ),
    );
    assertCleanupAttempts(cleanupAttempts, [{
      name: "legacy M0 record registry",
      check: () => assertRegistryClear(registry, "legacy M0 record registry"),
    }]);
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "independent feature catalog lifecycle",
      fn: testIndependentFeatureLifecycle,
    },
    {
      name: "catalog validation and nested caller mutation",
      fn: testCatalogValidationAndCallerMutation,
    },
    {
      name: "record identity, digest, and root definition validation",
      fn: testWrongRecordIdentityDigestAndRootConflict,
    },
    {
      name: "baseline M0 schema-2 install record reopens",
      fn: testM0RecordReopensWithoutSchemaChange,
    },
  ];
  await runTests("FeatureHostCatalog.test.mts", tests);
}
