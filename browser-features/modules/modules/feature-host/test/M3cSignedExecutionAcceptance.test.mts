// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert as harnessAssert,
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import type {
  Hooks,
  Host,
  SignedJournalForTests,
  SignedPackageAuthorization,
  SignedPackageFixture,
  SignedPackageOptions,
  SigningAuthority,
} from "./M3cSignedExecutionAcceptance.types.ts";

export const signedModule = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/M3cSignedExecution.sys.mjs",
) as typeof import("../M3cSignedExecution.sys.mts");
export const hostModule = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs",
) as typeof import("../FeatureHost.sys.mts");
export const encoder = new TextEncoder();
export const source =
  '(() => { let activations = 0; globalThis.__floorpM0 = { activate() { activations += 1; }, attachWindow() {}, health() { return "m3c:" + activations; }, deactivate() {} }; })();';

export function assert(condition: unknown, message: string): asserts condition {
  harnessAssert(condition, message);
}

export function assertEquals<T>(actual: T, expected: T, message: string): void {
  harnessAssertEquals(actual, expected, message);
}

export function hex(bytes: Uint8Array): string {
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join(
    "",
  );
}

export function arrayBuffer(bytes: Uint8Array): ArrayBuffer {
  const buffer = new ArrayBuffer(bytes.byteLength);
  new Uint8Array(buffer).set(bytes);
  return buffer;
}

export async function createSigningAuthority(): Promise<SigningAuthority> {
  const keyPair = await crypto.subtle.generateKey(
    { name: "Ed25519" },
    true,
    ["sign", "verify"],
  ) as CryptoKeyPair;
  const publicKey = btoa(String.fromCharCode(
    ...new Uint8Array(await crypto.subtle.exportKey("raw", keyPair.publicKey)),
  )).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/g, "");
  return { keyPair, keyId: "ephemeral-m3c-key", publicKey };
}

export function authorizationFor(
  authority: SigningAuthority,
  featureId: string,
  minimumReleaseSequence = 1,
): SignedPackageAuthorization {
  return {
    trustKind: "core-system-principal",
    policy: {
      featureId,
      hostSchema: 2,
      hostBuildId: "caller-value-is-ignored",
      runtimeBuildId: "caller-value-is-ignored",
      allowedCapabilities: [],
      minimumReleaseSequence,
      trustedKeys: [{
        keyId: authority.keyId,
        publicKey: authority.publicKey,
      }],
    },
  };
}

export async function signedPackage(
  options: SignedPackageOptions = {},
): Promise<SignedPackageFixture> {
  const authority = options.authority ?? await createSigningAuthority();
  const sourceText = options.source ?? source;
  const sourceBytes = encoder.encode(sourceText);
  const featureId = options.featureId ?? "m3c-signed-test";
  const version = options.version ?? "same-display-label";
  const releaseSequence = options.releaseSequence ?? 1;
  const compatibility = {
    hostSchema: 2,
    hostBuildId: "m0-schema-2",
    runtimeBuildIds: [Services.appinfo.appBuildID],
    capabilities: [],
  };
  const manifest = {
    featureId,
    version,
    releaseSequence,
    sourceBytes: sourceBytes.length,
    sourceSha256: hex(
      new Uint8Array(
        await crypto.subtle.digest("SHA-256", arrayBuffer(sourceBytes)),
      ),
    ),
    compatibility,
  };
  const signingBytes = encoder.encode(
    "floorp-feature-package:v1\n" +
      JSON.stringify({ formatVersion: 1, keyId: authority.keyId, manifest }),
  );
  const signature = btoa(String.fromCharCode(
    ...new Uint8Array(
      await crypto.subtle.sign(
        { name: "Ed25519" },
        authority.keyPair.privateKey,
        arrayBuffer(signingBytes),
      ),
    ),
  )).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/g, "");
  const bytes = encoder.encode(JSON.stringify({
    formatVersion: 1,
    keyId: authority.keyId,
    manifest,
    source: sourceText,
    signature,
  }));
  return {
    bytes,
    authorization: authorizationFor(
      authority,
      featureId,
      options.minimumReleaseSequence,
    ),
  };
}

export function testId(): string {
  return Services.uuid.generateUUID().toString().slice(1, -1).toLowerCase();
}

export function ownedRoot(name: string, id: string): nsIFile {
  const root = Services.dirsvc.get("ProfD", Components.interfaces.nsIFile);
  root.append(name);
  if (name === "floorp-signed-package-cache-v1") root.append("tests");
  root.append(id);
  return root;
}

export function featureRoot(
  name: string,
  id: string,
  featureId: string,
): nsIFile {
  const root = ownedRoot(name, id);
  root.append(featureId);
  return root;
}

export function journalFile(id: string, featureId: string): nsIFile {
  const file = featureRoot(
    "floorp-hotfix-signed-tests-v1",
    id,
    featureId,
  );
  file.append("state.json");
  return file;
}

export async function readJournal(
  id: string,
  featureId: string,
): Promise<SignedJournalForTests> {
  const value: unknown = await IOUtils.readJSON(
    journalFile(id, featureId).path,
  );
  assert(
    typeof value === "object" && value !== null && !Array.isArray(value),
    "journal is not an object",
  );
  return value as SignedJournalForTests;
}

export async function assertFacadeRejects(
  promise: Promise<unknown>,
  code: string,
): Promise<InstanceType<typeof signedModule.M3cSignedFacadeError>> {
  try {
    await promise;
  } catch (error) {
    assert(
      error instanceof signedModule.M3cSignedFacadeError,
      "expected a signed facade error",
    );
    assertEquals(error.code, code, "facade error category");
    return error;
  }
  throw new Error("expected facade rejection");
}

export async function removeTestRoots(id: string): Promise<void> {
  await IOUtils.remove(
    ownedRoot("floorp-hotfix-signed-tests-v1", id).path,
    { recursive: true, ignoreAbsent: true },
  );
  await IOUtils.remove(
    ownedRoot("floorp-signed-package-cache-v1", id).path,
    { recursive: true, ignoreAbsent: true },
  );
}

export function bytesEqual(left: Uint8Array, right: Uint8Array): boolean {
  return left.length === right.length &&
    left.every((value, index) => value === right[index]);
}

async function testSignedStageInstallEnableAndReopen(): Promise<void> {
  const id = testId();
  const featureId = "m3c-signed-test";
  const fixture = await signedPackage();
  const baselineEvaluations = hostModule.m0PayloadEvaluationsForTests();
  let host: Host | null = null;
  let reopened: Host | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      fixture.authorization,
      { excludeWindowsForTests: true },
    );
    const staged = await host.stage(fixture.bytes);
    assertEquals(staged.version, "same-display-label", "display label");
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      baselineEvaluations,
      "stage evaluates no source",
    );
    const stagedJournal = await readJournal(id, featureId);
    assertEquals(stagedJournal.lifecycle.stateRevision, 0, "stage revision");
    assertEquals(stagedJournal.highWater, null, "stage high-water");
    assertEquals(stagedJournal.packages.length, 0, "stage journal metadata");
    const installed = await host.install(
      "m3c-install",
      0,
      staged.packageSha256,
    );
    assertEquals(installed.status, "succeeded", "disabled install");
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      baselineEvaluations,
      "disabled install evaluates no source",
    );
    const installedDiagnostics = host.diagnostics() as {
      revision: number;
      selectedPackageSha256: string | null;
    };
    assertEquals(installedDiagnostics.revision, 2, "install journal revision");
    const installedJournal = await readJournal(id, featureId);
    assertEquals(
      installedJournal.lifecycle.stateRevision,
      2,
      "install journal boundary",
    );
    assertEquals(
      installedJournal.highWater?.releaseSequence,
      1,
      "install release floor",
    );
    assertEquals(
      installedJournal.highWater?.packageSha256,
      staged.packageSha256,
      "install release identity",
    );
    const enabled = await host.enable(
      "m3c-enable",
      installedDiagnostics.revision,
    );
    assertEquals(enabled.status, "succeeded", "enable");
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      baselineEvaluations + 1,
      "enable evaluates source once",
    );
    const diagnostics = host.diagnostics() as {
      revision: number;
      selectedPackageSha256: string | null;
    };
    assertEquals(
      diagnostics.selectedPackageSha256,
      staged.packageSha256,
      "package hash is lifecycle identity",
    );
    const closePromise = host.close();
    assert(host.close() === closePromise, "close promise is shared");
    await closePromise;
    host = null;

    const reopenedHost = await signedModule.reopenSignedFeatureHostForTests(
      id,
      featureId,
      true,
      fixture.authorization,
      { excludeWindowsForTests: true },
    );
    reopened = reopenedHost;
    const reopenedDiagnostics = reopenedHost.diagnostics() as {
      selectedPackageSha256: string | null;
    };
    assertEquals(
      reopenedDiagnostics.selectedPackageSha256,
      staged.packageSha256,
      "close/reopen retains package identity",
    );
    await reopenedHost.close();
    reopened = null;
    clean = true;
  } finally {
    if (host) await host.close();
    if (reopened) await reopened.close();
    if (clean) {
      await IOUtils.remove(
        ownedRoot("floorp-hotfix-signed-tests-v1", id).path,
        { recursive: true, ignoreAbsent: true },
      );
      await IOUtils.remove(
        ownedRoot("floorp-signed-package-cache-v1", id).path,
        { recursive: true, ignoreAbsent: true },
      );
    }
  }
}

async function testStageMustBeRestagedAfterReopen(): Promise<void> {
  const id = testId();
  const featureId = "m3c-signed-test";
  const fixture = await signedPackage();
  let host:
    | Awaited<
      ReturnType<typeof signedModule.createSignedFeatureHostForTests>
    >
    | null = null;
  let reopened:
    | Awaited<
      ReturnType<typeof signedModule.createSignedFeatureHostForTests>
    >
    | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      fixture.authorization,
      { excludeWindowsForTests: true },
    );
    const staged = await host.stage(fixture.bytes);
    await host.close();
    host = null;

    reopened = await signedModule.reopenSignedFeatureHostForTests(
      id,
      featureId,
      true,
      fixture.authorization,
      { excludeWindowsForTests: true },
    );
    assertEquals(
      reopened.metadata().length,
      0,
      "stage metadata is not journaled",
    );
    const restaged = await reopened.stage(fixture.bytes);
    assertEquals(
      restaged.packageSha256,
      staged.packageSha256,
      "restage returns the same package identity",
    );
    await reopened.close();
    reopened = null;
    clean = true;
  } finally {
    if (host) await host.close();
    if (reopened) await reopened.close();
    if (clean) {
      await IOUtils.remove(
        ownedRoot("floorp-hotfix-signed-tests-v1", id).path,
        { recursive: true, ignoreAbsent: true },
      );
      await IOUtils.remove(
        ownedRoot("floorp-signed-package-cache-v1", id).path,
        { recursive: true, ignoreAbsent: true },
      );
    }
  }
}

async function testSequenceFloorRollbackReplayAndFailedRestoration(): Promise<
  void
> {
  const id = testId();
  const featureId = "m3c-signed-test";
  const authority = await createSigningAuthority();
  const firstPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 1,
  });
  const secondPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 2,
  });
  const lowerPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 1,
    version: "lower-envelope",
  });
  const equivocalPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 2,
    version: "equivocal-envelope",
  });
  const failedPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 3,
    source: 'throw new Error("m3c candidate failure");',
  });
  const baselineEvaluations = hostModule.m0PayloadEvaluationsForTests();
  let host: Host | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      firstPackage.authorization,
      { excludeWindowsForTests: true },
    );
    const first = await host.stage(firstPackage.bytes);
    const installed = await host.install(
      "m3c-sequence-one-install",
      0,
      first.packageSha256,
    );
    assertEquals(installed.status, "succeeded", "sequence one install");
    let diagnostics = host.diagnostics() as {
      revision: number;
      selectedPackageSha256: string | null;
    };
    assertEquals(diagnostics.revision, 2, "sequence one install revision");
    let journal = await readJournal(id, featureId);
    assertEquals(journal.highWater?.releaseSequence, 1, "sequence one floor");
    assertEquals(
      journal.highWater?.packageSha256,
      first.packageSha256,
      "sequence one identity",
    );

    const enabled = await host.enable(
      "m3c-sequence-one-enable",
      diagnostics.revision,
    );
    assertEquals(enabled.status, "succeeded", "sequence one enable");
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      baselineEvaluations + 1,
      "sequence one evaluated once",
    );

    const second = await host.stage(secondPackage.bytes);
    const replayedStage = await host.stage(secondPackage.bytes);
    assertEquals(
      second.packageSha256,
      replayedStage.packageSha256,
      "same envelope retains identity",
    );
    assertEquals(
      first.version,
      second.version,
      "same source label is permitted",
    );
    assert(
      first.packageSha256 !== second.packageSha256,
      "different envelopes must have distinct identities",
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      baselineEvaluations + 1,
      "staging and exact replay do not evaluate source",
    );

    diagnostics = host.diagnostics() as {
      revision: number;
      selectedPackageSha256: string | null;
    };
    const updateRevision = diagnostics.revision;
    const update = await host.update(
      "m3c-sequence-two-update",
      updateRevision,
      second.packageSha256,
    );
    assertEquals(update.status, "succeeded", "sequence two update");
    diagnostics = host.diagnostics() as {
      revision: number;
      selectedPackageSha256: string | null;
    };
    journal = await readJournal(id, featureId);
    assertEquals(journal.highWater?.releaseSequence, 2, "sequence two floor");
    assertEquals(
      journal.highWater?.packageSha256,
      second.packageSha256,
      "sequence two identity",
    );
    const evaluationsAfterUpdate = hostModule.m0PayloadEvaluationsForTests();
    assertEquals(
      evaluationsAfterUpdate,
      baselineEvaluations + 2,
      "sequence two update evaluates once",
    );

    const replay = await host.update(
      "m3c-sequence-two-update",
      updateRevision,
      second.packageSha256,
    );
    assertEquals(replay.status, update.status, "transaction replay result");
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      evaluationsAfterUpdate,
      "transaction replay does not evaluate source",
    );
    assertEquals(
      (await readJournal(id, featureId)).lifecycle.stateRevision,
      diagnostics.revision,
      "transaction replay does not write the journal",
    );

    const rollback = await host.rollback(
      "m3c-sequence-one-rollback",
      diagnostics.revision,
    );
    assertEquals(rollback.status, "succeeded", "exact sequence one rollback");
    diagnostics = host.diagnostics() as {
      revision: number;
      selectedPackageSha256: string | null;
    };
    assertEquals(
      diagnostics.selectedPackageSha256,
      first.packageSha256,
      "rollback restores sequence one identity",
    );
    journal = await readJournal(id, featureId);
    assertEquals(journal.highWater?.releaseSequence, 2, "rollback keeps floor");
    assertEquals(
      journal.highWater?.packageSha256,
      second.packageSha256,
      "rollback keeps sequence two identity at floor",
    );

    const revisionBeforeRejectedAdmissions = diagnostics.revision;
    const evaluationsBeforeRejectedAdmissions = hostModule
      .m0PayloadEvaluationsForTests();
    const lower = await host.stage(lowerPackage.bytes);
    await assertFacadeRejects(
      host.update(
        "m3c-new-lower-admission",
        revisionBeforeRejectedAdmissions,
        lower.packageSha256,
      ),
      "sequence-too-low",
    );
    const equivocal = await host.stage(equivocalPackage.bytes);
    await assertFacadeRejects(
      host.update(
        "m3c-same-sequence-equivocation",
        revisionBeforeRejectedAdmissions,
        equivocal.packageSha256,
      ),
      "sequence-equivocation",
    );
    diagnostics = host.diagnostics() as {
      revision: number;
      selectedPackageSha256: string | null;
    };
    assertEquals(
      diagnostics.revision,
      revisionBeforeRejectedAdmissions,
      "rejected admissions do not advance revision",
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      evaluationsBeforeRejectedAdmissions,
      "rejected admissions do not evaluate source",
    );
    journal = await readJournal(id, featureId);
    assertEquals(
      journal.highWater?.releaseSequence,
      2,
      "rejected admissions preserve floor",
    );

    const failed = await host.stage(failedPackage.bytes);
    const evaluationsBeforeFailure = hostModule.m0PayloadEvaluationsForTests();
    const failedUpdate = await host.update(
      "m3c-sequence-three-failed",
      diagnostics.revision,
      failed.packageSha256,
    );
    assertEquals(
      failedUpdate.status,
      "failed_restored",
      "sequence three restores the previous healthy runtime",
    );
    diagnostics = host.diagnostics() as {
      revision: number;
      selectedPackageSha256: string | null;
    };
    assertEquals(
      diagnostics.selectedPackageSha256,
      first.packageSha256,
      "failed sequence three retains sequence one selection",
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      evaluationsBeforeFailure + 2,
      "failed candidate and restored runtime are evaluated exactly once each",
    );
    journal = await readJournal(id, featureId);
    assertEquals(journal.highWater?.releaseSequence, 3, "failed update floor");
    assertEquals(
      journal.highWater?.packageSha256,
      failed.packageSha256,
      "failed update identity remains at floor",
    );
    await host.close();
    host = null;
    clean = true;
  } finally {
    if (host) await host.close();
    if (clean) await removeTestRoots(id);
  }
}

async function testAuthorizationGetterReentrySnapshotsPolicyAndClose(): Promise<
  void
> {
  const id = testId();
  const featureId = "m3c-signed-test";
  const authority = await createSigningAuthority();
  const fixture = await signedPackage({ authority, featureId });
  let reentryCode: string | null = null;
  const authorization = {
    trustKind: "core-system-principal" as const,
    get policy() {
      try {
        signedModule.createSignedFeatureHostForTests(
          id,
          featureId,
          true,
          fixture.authorization,
        );
      } catch (error) {
        if (error instanceof signedModule.M3cSignedFacadeError) {
          reentryCode = error.code;
        }
      }
      return fixture.authorization.policy;
    },
  };
  let host: Host | null = null;
  let reopened: Host | null = null;
  let clean = false;
  const baselineEvaluations = hostModule.m0PayloadEvaluationsForTests();
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
      { excludeWindowsForTests: true },
    );
    assertEquals(reentryCode, "owner-active", "getter reentry is rejected");
    Reflect.set(fixture.authorization.policy, "minimumReleaseSequence", 99);
    await host.stage(fixture.bytes);
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      baselineEvaluations,
      "snapshotted policy staging is inert",
    );
    await host.close();
    host = null;

    reopened = await signedModule.reopenSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorizationFor(authority, featureId),
      { excludeWindowsForTests: true },
    );
    await reopened.close();
    reopened = null;
    clean = true;
  } finally {
    if (host) await host.close();
    if (reopened) await reopened.close();
    if (clean) await removeTestRoots(id);
  }
}

async function testPolicyArrayBoundsPrecedeElementReads(): Promise<void> {
  const id = testId();
  const featureId = "m3c-signed-test";
  const fixture = await signedPackage();
  let capabilityReads = 0;
  const hugeCapabilities = new Proxy([] as string[], {
    get(target, property, receiver) {
      if (property === "length") return 17;
      if (typeof property === "string" && /^\d+$/.test(property)) {
        capabilityReads += 1;
      }
      return Reflect.get(target, property, receiver);
    },
  });
  const capabilitiesAuthorization = {
    trustKind: "core-system-principal" as const,
    policy: {
      ...fixture.authorization.policy,
      allowedCapabilities: hugeCapabilities,
    },
  };
  await assertFacadeRejects(
    signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      capabilitiesAuthorization,
    ),
    "invalid-policy",
  );
  assertEquals(capabilityReads, 0, "oversized capability elements were read");
  assert(
    !featureRoot("floorp-hotfix-signed-tests-v1", id, featureId).exists(),
    "invalid policy created a journal root",
  );
  assert(
    !featureRoot("floorp-signed-package-cache-v1", id, featureId).exists(),
    "invalid policy created a cache root",
  );

  let keyReads = 0;
  const hugeKeys = new Proxy(
    [] as { keyId: string; publicKey: string }[],
    {
      get(target, property, receiver) {
        if (property === "length") return 17;
        if (typeof property === "string" && /^\d+$/.test(property)) {
          keyReads += 1;
        }
        return Reflect.get(target, property, receiver);
      },
    },
  );
  const keysAuthorization = {
    trustKind: "core-system-principal" as const,
    policy: {
      ...fixture.authorization.policy,
      trustedKeys: hugeKeys,
    },
  };
  await assertFacadeRejects(
    signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      keysAuthorization,
    ),
    "invalid-policy",
  );
  assertEquals(keyReads, 0, "oversized trusted-key elements were read");
}

async function testInvalidModeAndInternalRootHaveNoFilesystemEffects(): Promise<
  void
> {
  const featureId = "m3c-signed-test";
  const fixture = await signedPackage();
  const modeId = testId();
  const previousMode = Services.prefs.getStringPref("nora.startup.mode", "");
  const hadUserMode = Services.prefs.prefHasUserValue("nora.startup.mode");
  let syncError: unknown = null;
  let unexpectedPromise: Promise<unknown> | null = null;
  try {
    Services.prefs.setStringPref("nora.startup.mode", "release");
    try {
      unexpectedPromise = signedModule.createSignedFeatureHostForTests(
        modeId,
        featureId,
        true,
        fixture.authorization,
      );
    } catch (error) {
      syncError = error;
    }
    assert(
      syncError instanceof signedModule.M3cSignedFacadeError,
      "non-dev mode was not rejected synchronously",
    );
    assertEquals(syncError.code, "dev-test-only", "startup mode category");
    if (unexpectedPromise) await unexpectedPromise.catch(() => undefined);
    assert(
      !featureRoot(
        "floorp-hotfix-signed-tests-v1",
        modeId,
        featureId,
      ).exists(),
      "non-dev mode created a journal root",
    );
    assert(
      !featureRoot(
        "floorp-signed-package-cache-v1",
        modeId,
        featureId,
      ).exists(),
      "non-dev mode created a cache root",
    );
  } finally {
    if (hadUserMode) {
      Services.prefs.setStringPref("nora.startup.mode", previousMode);
    } else {
      Services.prefs.clearUserPref("nora.startup.mode");
    }
  }

  const directId = testId();
  const options = {
    testId: directId,
    identityCatalog: { featureId },
  } as unknown as Parameters<
    typeof hostModule.M0FeatureHost.openSignedForTests
  >[2];
  let rejected = false;
  try {
    await hostModule.M0FeatureHost.openSignedForTests(
      "/not-the-derived-signed-root",
      true,
      options,
    );
  } catch (error) {
    assert(
      typeof error === "object" && error !== null &&
        "message" in error && typeof error.message === "string",
      "direct root validation error type",
    );
    assertEquals(
      error.message,
      "invalid signed test identity",
      "direct root validation category",
    );
    rejected = true;
  }
  assert(rejected, "internal open accepted an unrelated root");
  assert(
    !featureRoot(
      "floorp-hotfix-signed-tests-v1",
      directId,
      featureId,
    ).exists(),
    "invalid direct root created a journal root",
  );
}

async function testSafeErrorsAreFreshAndBoundedForHostileThrows(): Promise<
  void
> {
  const featureId = "m3c-signed-test";
  const fixture = await signedPackage();
  const hooksThatThrow = (thrown: unknown): Hooks =>
    Object.defineProperty({}, "excludeWindowsForTests", {
      get() {
        throw thrown;
      },
    }) as Hooks;
  const forged = { code: "private-error-text", message: "private detail" };
  const branded = new signedModule.M3cSignedFacadeError("invalid-policy");
  let stringificationAttempts = 0;
  const hostileProxy = new Proxy({}, {
    getPrototypeOf() {
      throw new Error("hostile prototype trap");
    },
    get(_target, property) {
      if (property === Symbol.toPrimitive) {
        stringificationAttempts += 1;
        return () => {
          stringificationAttempts += 1;
          throw new Error("hostile coercion");
        };
      }
      return undefined;
    },
  });
  const cases: {
    readonly thrown: unknown;
    readonly expectedCode: string;
  }[] = [
    { thrown: forged, expectedCode: "operation-failed" },
    { thrown: branded, expectedCode: "invalid-policy" },
    { thrown: hostileProxy, expectedCode: "operation-failed" },
  ];
  for (const [index, testCase] of cases.entries()) {
    const id = testId();
    const outward = await assertFacadeRejects(
      signedModule.createSignedFeatureHostForTests(
        id,
        featureId,
        true,
        fixture.authorization,
        hooksThatThrow(testCase.thrown),
      ),
      testCase.expectedCode,
    );
    assert(outward !== testCase.thrown, "outward error was not fresh");
    assert(
      !featureRoot("floorp-hotfix-signed-tests-v1", id, featureId).exists(),
      "hostile error created a journal root",
    );
    if (index === 2) {
      assertEquals(
        stringificationAttempts,
        0,
        "hostile proxy was stringified",
      );
    }
  }
}

async function testChangedMinimumPolicyOnReopenPreservesJournalFloor(): Promise<
  void
> {
  const id = testId();
  const featureId = "m3c-policy-reopen";
  const authority = await createSigningAuthority();
  const fixture = await signedPackage({ authority, featureId });
  const baselineEvaluations = hostModule.m0PayloadEvaluationsForTests();
  let host: Host | null = null;
  let reopened: Host | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      fixture.authorization,
      { excludeWindowsForTests: true },
    );
    const staged = await host.stage(fixture.bytes);
    const installed = await host.install(
      "m3c-policy-install",
      0,
      staged.packageSha256,
    );
    assertEquals(installed.status, "succeeded", "policy package install");
    const beforeEnable = host.diagnostics() as { revision: number };
    const enabled = await host.enable(
      "m3c-policy-enable",
      beforeEnable.revision,
    );
    assertEquals(enabled.status, "succeeded", "policy package enable");
    const evaluationsBeforeReopen = hostModule.m0PayloadEvaluationsForTests();
    await host.close();
    host = null;

    reopened = await signedModule.reopenSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorizationFor(authority, featureId, 2),
      { excludeWindowsForTests: true },
    );
    const diagnostics = reopened.diagnostics() as {
      blocked: boolean;
      selectedPackageSha256: string | null;
    };
    assert(diagnostics.blocked, "raised minimum did not block the old package");
    assertEquals(
      diagnostics.selectedPackageSha256,
      staged.packageSha256,
      "reopen retained the recorded package identity",
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      evaluationsBeforeReopen,
      "current minimum rejection evaluated no source",
    );
    const journal = await readJournal(id, featureId);
    assertEquals(journal.highWater?.releaseSequence, 1, "policy reopen floor");
    assertEquals(
      journal.highWater?.packageSha256,
      staged.packageSha256,
      "policy reopen identity",
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests() > baselineEvaluations,
      true,
      "initial healthy version was evaluated",
    );
    await reopened.close();
    reopened = null;
    clean = true;
  } finally {
    if (host) await host.close();
    if (reopened) await reopened.close();
    if (clean) await removeTestRoots(id);
  }
}

async function testTwoSignedFeaturesRemainIsolated(): Promise<void> {
  const id = testId();
  const firstFeature = "m3c-isolation-first";
  const secondFeature = "m3c-isolation-second";
  const authority = await createSigningAuthority();
  const firstPackage = await signedPackage({
    authority,
    featureId: firstFeature,
  });
  const secondPackage = await signedPackage({
    authority,
    featureId: secondFeature,
  });
  const baselineEvaluations = hostModule.m0PayloadEvaluationsForTests();
  let firstHost: Host | null = null;
  let secondHost: Host | null = null;
  let clean = false;
  try {
    firstHost = await signedModule.createSignedFeatureHostForTests(
      id,
      firstFeature,
      true,
      firstPackage.authorization,
      { excludeWindowsForTests: true },
    );
    secondHost = await signedModule.createSignedFeatureHostForTests(
      id,
      secondFeature,
      true,
      secondPackage.authorization,
      { excludeWindowsForTests: true },
    );
    const first = await firstHost.stage(firstPackage.bytes);
    const second = await secondHost.stage(secondPackage.bytes);
    const firstInstall = await firstHost.install(
      "m3c-isolated-first-install",
      0,
      first.packageSha256,
    );
    const secondInstall = await secondHost.install(
      "m3c-isolated-second-install",
      0,
      second.packageSha256,
    );
    assertEquals(firstInstall.status, "succeeded", "first feature install");
    assertEquals(secondInstall.status, "succeeded", "second feature install");
    const firstBeforeEnable = firstHost.diagnostics() as { revision: number };
    const enabled = await firstHost.enable(
      "m3c-isolated-first-enable",
      firstBeforeEnable.revision,
    );
    assertEquals(enabled.status, "succeeded", "first feature enable");
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      baselineEvaluations + 1,
      "only the enabled feature evaluated",
    );
    const firstJournal = await readJournal(id, firstFeature);
    const secondJournal = await readJournal(id, secondFeature);
    assertEquals(
      firstJournal.highWater?.packageSha256,
      first.packageSha256,
      "first feature journal identity",
    );
    assertEquals(
      secondJournal.highWater?.packageSha256,
      second.packageSha256,
      "second feature journal identity",
    );
    assertEquals(
      firstJournal.lifecycle.desiredEnabled,
      true,
      "first feature desired state",
    );
    assertEquals(
      secondJournal.lifecycle.desiredEnabled,
      false,
      "second feature desired state",
    );
    await firstHost.close();
    firstHost = null;
    await secondHost.close();
    secondHost = null;
    clean = true;
  } finally {
    if (firstHost) await firstHost.close();
    if (secondHost) await secondHost.close();
    if (clean) await removeTestRoots(id);
  }
}

async function testPartialInitializationCleanupPreservesSiblingAndRetries(): Promise<
  void
> {
  const id = testId();
  const siblingFeature = "m3c-partial-sibling";
  const targetFeature = "m3c-partial-target";
  const authority = await createSigningAuthority();
  const siblingPackage = await signedPackage({
    authority,
    featureId: siblingFeature,
  });
  const targetPackage = await signedPackage({
    authority,
    featureId: targetFeature,
  });
  const baselineEvaluations = hostModule.m0PayloadEvaluationsForTests();
  let sibling: Host | null = null;
  let retry: Host | null = null;
  let clean = false;
  try {
    sibling = await signedModule.createSignedFeatureHostForTests(
      id,
      siblingFeature,
      true,
      siblingPackage.authorization,
      { excludeWindowsForTests: true },
    );
    await sibling.close();
    sibling = null;
    assert(
      featureRoot(
        "floorp-hotfix-signed-tests-v1",
        id,
        siblingFeature,
      ).exists(),
      "sibling journal root was not created",
    );
    assert(
      featureRoot(
        "floorp-signed-package-cache-v1",
        id,
        siblingFeature,
      ).exists(),
      "sibling cache root was not created",
    );

    await assertFacadeRejects(
      signedModule.createSignedFeatureHostForTests(
        id,
        targetFeature,
        true,
        targetPackage.authorization,
        {
          excludeWindowsForTests: true,
          failInitialJournalForTests: true,
        },
      ),
      "invalid-journal",
    );
    assert(
      !featureRoot(
        "floorp-hotfix-signed-tests-v1",
        id,
        targetFeature,
      ).exists(),
      "partial journal feature root was not removed",
    );
    assert(
      !featureRoot(
        "floorp-signed-package-cache-v1",
        id,
        targetFeature,
      ).exists(),
      "partial cache feature root was not removed",
    );
    assert(
      featureRoot(
        "floorp-hotfix-signed-tests-v1",
        id,
        siblingFeature,
      ).exists(),
      "partial cleanup removed sibling journal",
    );
    assert(
      featureRoot(
        "floorp-signed-package-cache-v1",
        id,
        siblingFeature,
      ).exists(),
      "partial cleanup removed sibling cache",
    );

    retry = await signedModule.createSignedFeatureHostForTests(
      id,
      targetFeature,
      true,
      targetPackage.authorization,
      { excludeWindowsForTests: true },
    );
    await retry.close();
    retry = null;
    clean = true;
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      baselineEvaluations,
      "partial initialization and retry evaluate no source",
    );
  } finally {
    if (sibling) await sibling.close();
    if (retry) await retry.close();
    if (clean) await removeTestRoots(id);
  }
}

async function testMissingAndCorruptJournalReopenDoesNotRewrite(): Promise<
  void
> {
  const id = testId();
  const featureId = "m3c-journal-reopen";
  const fixture = await signedPackage({ featureId });
  const baselineEvaluations = hostModule.m0PayloadEvaluationsForTests();
  let host: Host | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      fixture.authorization,
      { excludeWindowsForTests: true },
    );
    await host.close();
    host = null;
    const recordFile = journalFile(id, featureId);
    const original = await IOUtils.read(recordFile.path);
    await IOUtils.remove(recordFile.path);
    await assertFacadeRejects(
      signedModule.reopenSignedFeatureHostForTests(
        id,
        featureId,
        true,
        fixture.authorization,
        { excludeWindowsForTests: true },
      ),
      "invalid-journal",
    );
    assert(!recordFile.exists(), "missing journal was recreated");

    await IOUtils.write(recordFile.path, original);
    const corrupt = encoder.encode("{");
    await IOUtils.write(recordFile.path, corrupt);
    await assertFacadeRejects(
      signedModule.reopenSignedFeatureHostForTests(
        id,
        featureId,
        true,
        fixture.authorization,
        { excludeWindowsForTests: true },
      ),
      "invalid-journal",
    );
    assert(
      bytesEqual(await IOUtils.read(recordFile.path), corrupt),
      "corrupt journal was overwritten",
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      baselineEvaluations,
      "invalid journal reopen evaluates no source",
    );
    clean = true;
  } finally {
    if (host) await host.close();
    if (clean) await removeTestRoots(id);
  }
}

const tests: TestCase[] = [
  {
    name:
      "signed stage is inert, install/enable and reopen use package identity",
    fn: testSignedStageInstallEnableAndReopen,
  },
  {
    name: "stage-only metadata must be restaged after close and reopen",
    fn: testStageMustBeRestagedAfterReopen,
  },
  {
    name:
      "signed sequence floors survive rollback, replay, rejected admission and restoration",
    fn: testSequenceFloorRollbackReplayAndFailedRestoration,
  },
  {
    name:
      "authorization getters cannot reenter an owner and policy is snapshotted",
    fn: testAuthorizationGetterReentrySnapshotsPolicyAndClose,
  },
  {
    name: "policy array bounds are checked before element reads",
    fn: testPolicyArrayBoundsPrecedeElementReads,
  },
  {
    name: "mode and internal signed-root checks precede filesystem effects",
    fn: testInvalidModeAndInternalRootHaveNoFilesystemEffects,
  },
  {
    name: "facade errors are fresh and bounded for hostile thrown values",
    fn: testSafeErrorsAreFreshAndBoundedForHostileThrows,
  },
  {
    name: "reopen uses the changed current minimum without lowering the floor",
    fn: testChangedMinimumPolicyOnReopenPreservesJournalFloor,
  },
  {
    name: "two signed feature IDs have isolated journals and lifecycle state",
    fn: testTwoSignedFeaturesRemainIsolated,
  },
  {
    name: "partial initialization cleanup preserves siblings and permits retry",
    fn: testPartialInitializationCleanupPreservesSiblingAndRetries,
  },
  {
    name: "missing and corrupt journal reopen does not rewrite state",
    fn: testMissingAndCorruptJournalReopenDoesNotRewrite,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("M3cSignedExecutionAcceptance.test.mts", tests);
}
