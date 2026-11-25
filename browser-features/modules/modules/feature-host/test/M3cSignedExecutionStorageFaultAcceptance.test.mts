// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  assertFacadeRejects,
  authorizationFor,
  bytesEqual,
  createSigningAuthority,
  encoder,
  featureRoot,
  hostModule,
  journalFile,
  ownedRoot,
  readJournal,
  removeTestRoots,
  signedModule,
  signedPackage,
  testId,
} from "./M3cSignedExecutionAcceptance.test.mts";
import type {
  Hooks,
  Host,
  SignedJournalForTests,
  SignedPackageAuthorization,
  SigningAuthority,
} from "./M3cSignedExecutionAcceptance.types.ts";
import {
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

const featureId = "m3c-storage-fault-test";
const cacheRootName = "floorp-signed-package-cache-v1";
const journalRootName = "floorp-hotfix-signed-tests-v1";

function revision(host: Host): number {
  return Number(host.diagnostics().revision);
}

function asRecord(value: unknown): Record<string, unknown> {
  assert(
    typeof value === "object" && value !== null && !Array.isArray(value),
    "value is a record",
  );
  return value as Record<string, unknown>;
}

function cacheFile(id: string, hash: string): nsIFile {
  const file = featureRoot(cacheRootName, id, featureId);
  file.append(hash);
  return file;
}

function providerAuthorization(
  authorities: readonly SigningAuthority[],
): SignedPackageAuthorization {
  const first = authorities[0];
  assert(first, "provider policy has at least one trusted signer");
  const base = authorizationFor(first, featureId);
  return {
    ...base,
    policy: {
      ...base.policy,
      trustedKeys: authorities.map(({ keyId, publicKey }) => ({
        keyId,
        publicKey,
      })),
    },
  };
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

async function supportsPermissionDeniedCacheIo(): Promise<boolean> {
  if (Services.appinfo.OS !== "Linux") return false;
  try {
    const status = new TextDecoder().decode(
      await IOUtils.read("/proc/self/status"),
    );
    const effectiveUid = /^Uid:\s+\d+\s+(\d+)/m.exec(status)?.[1];
    return effectiveUid !== undefined && effectiveUid !== "0";
  } catch {
    return false;
  }
}

async function assertReopenFacadeRejects(
  open: () => Promise<Host>,
  code: string,
): Promise<void> {
  let opened: Host | null = null;
  let failure: unknown;
  try {
    opened = await open();
  } catch (error: unknown) {
    failure = error;
  }
  if (opened) await opened.close();
  assert(
    failure instanceof signedModule.M3cSignedFacadeError,
    "reopen rejects with a bounded facade error",
  );
  assertEquals(failure.code, code, "reopen failure category is preserved");
}

function createSymlink(targetPath: string, linkPath: string): void {
  const executableFactory = Reflect.get(
    Components.classes,
    "@mozilla.org/file/local;1",
  ) as { createInstance(interfaceId: unknown): nsIFile };
  const executable = executableFactory.createInstance(
    Components.interfaces.nsIFile,
  );
  executable.initWithPath("/bin/ln");
  const processFactory = Reflect.get(
    Components.classes,
    "@mozilla.org/process/util;1",
  ) as {
    createInstance(interfaceId: unknown): {
      init(executable: nsIFile): void;
      run(blocking: boolean, args: string[], count: number): void;
      readonly exitValue: number;
    };
  };
  const process = processFactory.createInstance(
    Components.interfaces.nsIProcess,
  );
  process.init(executable);
  const args = ["-s", targetPath, linkPath];
  process.run(true, args, args.length);
  if (process.exitValue !== 0) {
    throw new Error("Could not create the controlled test symlink.");
  }
}

function assertNoAttempt(journal: SignedJournalForTests): void {
  const attempt = journal.lifecycle.attempt;
  if (attempt === null || attempt === undefined) return;
  const value = asRecord(attempt);
  assert(
    Array.isArray(value.attemptedDigests),
    "attempt ledger has attempted list",
  );
  assert(
    Array.isArray(value.rejectedDigests),
    "attempt ledger has rejected list",
  );
  assertEquals(value.attemptedDigests.length, 0, "no attempted candidates");
  assertEquals(value.rejectedDigests.length, 0, "no rejected candidates");
}

async function installAndEnable(
  host: Host,
  id: string,
  bytes: Uint8Array,
): Promise<{ readonly packageSha256: string }> {
  const metadata = await host.stage(bytes);
  assertEquals(
    (await host.install(
      `install-${id}`,
      revision(host),
      metadata.packageSha256,
    ))
      .status,
    "succeeded",
    "initial install succeeds",
  );
  assertEquals(
    (await host.enable(`enable-${id}`, revision(host))).status,
    "succeeded",
    "initial enable succeeds",
  );
  return metadata;
}

async function testBeforeReplacePreservesJournalAndRetries(): Promise<void> {
  const id = testId();
  const authority = await createSigningAuthority();
  const authorization = authorizationFor(authority, featureId);
  const oldPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 1,
  });
  const targetPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 2,
  });
  let host: Host | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    const oldMetadata = await installAndEnable(host, id, oldPackage.bytes);
    const targetMetadata = await host.stage(targetPackage.bytes);
    const journalPath = journalFile(id, featureId);
    const beforeBytes = await IOUtils.read(journalPath.path);
    const before = await readJournal(id, featureId);
    const evaluations = hostModule.m0PayloadEvaluationsForTests();
    await host.setNextWriteFaultForTests("before_replace");
    const failed = await host.update(
      `before-replace-${id}`,
      revision(host),
      targetMetadata.packageSha256,
    );
    assertEquals(
      failed.status,
      "storage_failed",
      "pre-replace write is reported",
    );
    assert(
      bytesEqual(await IOUtils.read(journalPath.path), beforeBytes),
      "pre-replace failure leaves journal bytes unchanged",
    );
    const after = await readJournal(id, featureId);
    assertEquals(
      after.highWater?.releaseSequence ?? null,
      before.highWater?.releaseSequence ?? null,
      "floor unchanged",
    );
    assertEquals(
      after.highWater?.packageSha256 ?? null,
      before.highWater?.packageSha256 ?? null,
      "floor identity unchanged",
    );
    assertNoAttempt(after);
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      evaluations,
      "pre-replace failure evaluates no candidate",
    );
    assertEquals(
      String(host.diagnostics().selectedPackageSha256),
      oldMetadata.packageSha256,
      "old selection remains usable",
    );
    assertEquals(
      host.diagnostics().blocked,
      false,
      "known pre-replace failure does not quarantine",
    );
    const retried = await host.update(
      `before-replace-${id}`,
      revision(host),
      targetMetadata.packageSha256,
    );
    assertEquals(
      retried.status,
      "succeeded",
      "known-uncommitted transaction can retry",
    );
    const committed = await readJournal(id, featureId);
    assertEquals(
      committed.highWater?.releaseSequence ?? null,
      2,
      "retry commits sequence 2 floor",
    );
    assertEquals(
      committed.highWater?.packageSha256 ?? null,
      targetMetadata.packageSha256,
      "retry commits target identity",
    );
    await host.close();
    host = null;
    clean = true;
  } finally {
    if (host) await host.close();
    if (clean) await removeTestRoots(id);
  }
}

async function testAfterReplacePreparedRestoresPreviousSelection(): Promise<
  void
> {
  const id = testId();
  const authority = await createSigningAuthority();
  const authorization = authorizationFor(authority, featureId);
  const oldPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 1,
  });
  const targetPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 2,
  });
  let host: Host | null = null;
  let reopened: Host | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    const oldMetadata = await installAndEnable(host, id, oldPackage.bytes);
    const targetMetadata = await host.stage(targetPackage.bytes);
    const evaluations = hostModule.m0PayloadEvaluationsForTests();
    await host.setNextWriteFaultForTests("after_replace");
    const failed = await host.update(
      `prepared-${id}`,
      revision(host),
      targetMetadata.packageSha256,
    );
    assertEquals(
      failed.status,
      "storage_failed",
      "observed PREPARED replacement reports storage failure",
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      evaluations,
      "PREPARED fault does not evaluate candidate",
    );
    const durable = await readJournal(id, featureId);
    assertEquals(
      durable.highWater?.releaseSequence ?? null,
      2,
      "PREPARED fault retains target floor",
    );
    assertEquals(
      durable.highWater?.packageSha256 ?? null,
      targetMetadata.packageSha256,
      "PREPARED fault retains target identity",
    );
    assertEquals(
      durable.lifecycle.selectedDigest,
      oldMetadata.packageSha256,
      "PREPARED retains previous selected identity",
    );
    assertEquals(
      durable.lifecycle.desiredEnabled,
      true,
      "PREPARED retains enabled state",
    );
    const pending = asRecord(durable.lifecycle.pending);
    assertEquals(
      pending.phase,
      "PREPARED",
      "durable state retains PREPARED phase",
    );
    assertNoAttempt(durable);
    await assertFacadeRejects(
      host.disable(`blocked-${id}`, revision(host)),
      "quarantined",
    );
    await host.close();
    host = null;
    const beforeRecoveryEvaluations = hostModule.m0PayloadEvaluationsForTests();
    reopened = await signedModule.reopenSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    assertEquals(
      String(reopened.diagnostics().selectedPackageSha256),
      oldMetadata.packageSha256,
      "reopen restores previous healthy selection",
    );
    assertEquals(
      reopened.diagnostics().blockCategory,
      null,
      "clean reopen clears volatile block",
    );
    const restored = await readJournal(id, featureId);
    assertEquals(
      restored.highWater?.releaseSequence ?? null,
      2,
      "recovery does not lower floor",
    );
    assertEquals(
      restored.highWater?.packageSha256 ?? null,
      targetMetadata.packageSha256,
      "recovery retains failed target identity",
    );
    assertEquals(
      restored.lifecycle.pending,
      null,
      "recovery clears PREPARED state",
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      beforeRecoveryEvaluations + 1,
      "only previous healthy selection is reevaluated",
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

async function runProviderPreflightFailure(
  kind: "unsafe" | "io",
): Promise<void> {
  const id = testId();
  const authority = await createSigningAuthority();
  const authorization = authorizationFor(authority, featureId);
  const oldPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 1,
  });
  const targetPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 2,
  });
  let host: Host | null = null;
  let reopened: Host | null = null;
  let originalPermissions = 0;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    const oldMetadata = await installAndEnable(host, id, oldPackage.bytes);
    const targetMetadata = await host.stage(targetPackage.bytes);
    const targetFile = cacheFile(id, targetMetadata.packageSha256);
    const beforeBytes = await IOUtils.read(journalFile(id, featureId).path);
    const evaluations = hostModule.m0PayloadEvaluationsForTests();
    originalPermissions = targetFile.permissions;
    if (kind === "unsafe") {
      await IOUtils.remove(targetFile.path);
      targetFile.create(Components.interfaces.nsIFile.DIRECTORY_TYPE!, 0o700);
    } else {
      targetFile.permissions = 0;
    }
    await assertFacadeRejects(
      host.update(
        `provider-${kind}-${id}`,
        revision(host),
        targetMetadata.packageSha256,
      ),
      kind === "unsafe" ? "provider-unsafe-layout" : "provider-cache-io",
    );
    assert(
      bytesEqual(
        await IOUtils.read(journalFile(id, featureId).path),
        beforeBytes,
      ),
      "pre-PREPARED provider fault leaves journal unchanged",
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      evaluations,
      "pre-PREPARED provider fault evaluates no candidate",
    );
    const journal = await readJournal(id, featureId);
    assertEquals(
      journal.highWater?.releaseSequence ?? null,
      1,
      "provider fault leaves floor unchanged",
    );
    assertNoAttempt(journal);
    assertEquals(
      host.diagnostics().blockCategory,
      kind === "unsafe" ? "unsafe-layout" : "cache-io",
      "provider reason is retained",
    );
    await assertFacadeRejects(
      host.disable(`blocked-${id}`, revision(host)),
      "quarantined",
    );
    await host.close();
    host = null;
    if (kind === "unsafe") {
      await IOUtils.remove(targetFile.path, { recursive: true });
    } else {
      targetFile.permissions = originalPermissions;
    }
    reopened = await signedModule.reopenSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    assertEquals(
      reopened.diagnostics().blockCategory,
      null,
      "close/reopen clears provider block",
    );
    assertEquals(
      String(reopened.diagnostics().selectedPackageSha256),
      oldMetadata.packageSha256,
      "old selection survives provider failure",
    );
    const restaged = await reopened.stage(targetPackage.bytes);
    assertEquals(
      restaged.packageSha256,
      targetMetadata.packageSha256,
      "cache artifact can be staged again after reopen",
    );
    assertEquals(
      (await reopened.disable(`reopened-${id}`, revision(reopened))).status,
      "succeeded",
      "mutations resume after clean reopen",
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

async function testCandidateUnavailableIsFiniteAndNonBlocking(): Promise<void> {
  const id = testId();
  const authority = await createSigningAuthority();
  const authorization = authorizationFor(authority, featureId);
  const oldPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 1,
  });
  const targetPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 2,
  });
  let host: Host | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    const oldMetadata = await installAndEnable(host, id, oldPackage.bytes);
    const targetMetadata = await host.stage(targetPackage.bytes);
    const journalPath = journalFile(id, featureId);
    const beforeBytes = await IOUtils.read(journalPath.path);
    const evaluations = hostModule.m0PayloadEvaluationsForTests();
    await IOUtils.remove(cacheFile(id, targetMetadata.packageSha256).path);
    await assertFacadeRejects(
      host.update(
        `missing-prepared-${id}`,
        revision(host),
        targetMetadata.packageSha256,
      ),
      "package-unavailable",
    );
    assert(
      bytesEqual(await IOUtils.read(journalPath.path), beforeBytes),
      "candidate-unavailable leaves journal unchanged",
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      evaluations,
      "candidate-unavailable evaluates no source",
    );
    assertEquals(
      host.diagnostics().blockCategory,
      null,
      "candidate-unavailable does not freeze engine",
    );
    assertEquals(
      host.diagnostics().blocked,
      false,
      "candidate-unavailable is not quarantine",
    );
    assertEquals(
      String(host.diagnostics().selectedPackageSha256),
      oldMetadata.packageSha256,
      "healthy selection remains active",
    );
    const restaged = await host.stage(targetPackage.bytes);
    assertEquals(
      restaged.packageSha256,
      targetMetadata.packageSha256,
      "missing artifact can be restaged",
    );
    assertEquals(
      (await host.update(
        `missing-prepared-${id}`,
        revision(host),
        targetMetadata.packageSha256,
      )).status,
      "succeeded",
      "engine remains mutable after candidate-unavailable",
    );
    await host.close();
    host = null;
    clean = true;
  } finally {
    if (host) await host.close();
    if (clean) await removeTestRoots(id);
  }
}

async function testStageOnlyFailureDoesNotQuarantine(): Promise<void> {
  const id = testId();
  const authority = await createSigningAuthority();
  const authorization = authorizationFor(authority, featureId);
  const oldPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 1,
  });
  let host: Host | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    const oldMetadata = await installAndEnable(host, id, oldPackage.bytes);
    const beforeBytes = await IOUtils.read(journalFile(id, featureId).path);
    const evaluations = hostModule.m0PayloadEvaluationsForTests();
    const forged = JSON.parse(
      new TextDecoder().decode(oldPackage.bytes),
    ) as Record<string, unknown>;
    forged.signature = "invalid-signature";
    await assertFacadeRejects(
      host.stage(encoder.encode(JSON.stringify(forged))),
      "package-unavailable",
    );
    assert(
      bytesEqual(
        await IOUtils.read(journalFile(id, featureId).path),
        beforeBytes,
      ),
      "stage-only failure does not write journal",
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      evaluations,
      "stage-only failure evaluates no source",
    );
    assertEquals(
      host.diagnostics().blockCategory,
      null,
      "stage-only failure does not set provider block",
    );
    assertEquals(
      String(host.diagnostics().selectedPackageSha256),
      oldMetadata.packageSha256,
      "existing runtime selection remains",
    );
    assertEquals(
      (await host.disable(`stage-failure-${id}`, revision(host))).status,
      "succeeded",
      "lifecycle remains mutable after stage failure",
    );
    await host.close();
    host = null;
    clean = true;
  } finally {
    if (host) await host.close();
    if (clean) await removeTestRoots(id);
  }
}

async function runRecoveryFallback(
  kind: "missing" | "corrupt" | "policy",
): Promise<void> {
  const id = testId();
  const oldAuthorityBase = await createSigningAuthority();
  const oldAuthority = { ...oldAuthorityBase, keyId: `fallback-${id}` };
  const targetAuthorityBase = kind === "policy"
    ? await createSigningAuthority()
    : oldAuthorityBase;
  const targetAuthority = kind === "policy"
    ? { ...targetAuthorityBase, keyId: `target-${id}` }
    : oldAuthority;
  const oldPackage = await signedPackage({
    authority: oldAuthority,
    featureId,
    releaseSequence: 1,
  });
  const targetPackage = await signedPackage({
    authority: targetAuthority,
    featureId,
    releaseSequence: 2,
  });
  const initialAuthorization = providerAuthorization(
    kind === "policy" ? [oldAuthority, targetAuthority] : [oldAuthority],
  );
  const currentAuthorization = kind === "policy"
    ? authorizationFor(oldAuthority, featureId)
    : initialAuthorization;
  let host: Host | null = null;
  let reopened: Host | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      initialAuthorization,
    );
    const oldMetadata = await installAndEnable(host, id, oldPackage.bytes);
    const targetMetadata = await host.stage(targetPackage.bytes);
    const updated = await host.update(
      `recovery-${kind}-${id}`,
      revision(host),
      targetMetadata.packageSha256,
    );
    assertEquals(
      updated.status,
      "succeeded",
      "target becomes the current selected candidate",
    );
    const selectedJournal = await readJournal(id, featureId);
    assertEquals(
      selectedJournal.lifecycle.selectedDigest,
      targetMetadata.packageSha256,
      "journal selects target before reopen",
    );
    assertEquals(
      selectedJournal.lifecycle.rollbackDigest,
      oldMetadata.packageSha256,
      "exact previous package has rollback health proof",
    );
    assertEquals(
      selectedJournal.highWater?.releaseSequence ?? null,
      2,
      "successful update records high-water sequence",
    );
    await host.close();
    host = null;
    const targetFile = cacheFile(id, targetMetadata.packageSha256);
    if (kind === "missing") {
      await IOUtils.remove(targetFile.path);
    } else if (kind === "corrupt") {
      await IOUtils.write(targetFile.path, new Uint8Array([0x7b, 0x7d]));
    }
    const observed: Readonly<Record<string, unknown>>[] = [];
    const hooks: Hooks = {
      afterDurableWriteForTests(snapshot) {
        observed.push(snapshot);
      },
    };
    const beforeRecoveryEvaluations = hostModule.m0PayloadEvaluationsForTests();
    reopened = await signedModule.reopenSignedFeatureHostForTests(
      id,
      featureId,
      true,
      currentAuthorization,
      hooks,
    );
    assertEquals(
      String(reopened.diagnostics().selectedPackageSha256),
      oldMetadata.packageSha256,
      `${kind} candidate restores exact eligible fallback`,
    );
    const recovered = await readJournal(id, featureId);
    assertEquals(
      recovered.highWater?.releaseSequence ?? null,
      2,
      "fallback does not lower release floor",
    );
    assertEquals(
      recovered.highWater?.packageSha256 ?? null,
      targetMetadata.packageSha256,
      "fallback retains target identity at high-water",
    );
    assertEquals(
      recovered.lifecycle.pending,
      null,
      "fallback recovery clears pending transaction",
    );
    assert(
      recovered.lifecycle.rollbackDigest !== targetMetadata.packageSha256,
      "rejected target is not retained as a healthy rollback target",
    );
    const ledgersStayBounded = observed.every((snapshot) => {
      const attempt = snapshot.attempt;
      if (attempt === null || attempt === undefined) return true;
      if (!isRecord(attempt)) return false;
      const attempted = attempt.attemptedDigests;
      const rejected = attempt.rejectedDigests;
      return Array.isArray(attempted) && Array.isArray(rejected) &&
        attempted.length + rejected.length <= 2;
    });
    assert(
      ledgersStayBounded,
      `every observed candidate ledger stays within two entries: ${
        JSON.stringify(observed.map((snapshot) => snapshot.attempt ?? null))
      }`,
    );
    const twoCandidateLedger = observed.some((snapshot) => {
      const attempt = snapshot.attempt;
      if (!isRecord(attempt)) return false;
      const attempted = attempt.attemptedDigests;
      const rejected = attempt.rejectedDigests;
      return Array.isArray(attempted) && Array.isArray(rejected) &&
        attempted.length + rejected.length === 2 &&
        attempted.includes(oldMetadata.packageSha256) &&
        rejected.includes(targetMetadata.packageSha256);
    });
    assert(
      twoCandidateLedger,
      `recovery records rejected target and exact fallback within two-candidate ledger: ${
        JSON.stringify(observed.map((snapshot) => snapshot.attempt ?? null))
      }`,
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      beforeRecoveryEvaluations + 1,
      "only eligible fallback source is evaluated",
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

async function testProviderRecoveryDoesNotConsumeLedger(): Promise<void> {
  const id = testId();
  const authority = await createSigningAuthority();
  const authorization = authorizationFor(authority, featureId);
  const oldPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 1,
  });
  const targetPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 2,
  });
  let host: Host | null = null;
  let reopened: Host | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    const oldMetadata = await installAndEnable(host, id, oldPackage.bytes);
    const targetMetadata = await host.stage(targetPackage.bytes);
    const evaluations = hostModule.m0PayloadEvaluationsForTests();
    const transactionId = `provider-recovery-${id}`;
    await host.setNextWriteFaultForTests("after_replace");
    assertEquals(
      (await host.update(
        transactionId,
        revision(host),
        targetMetadata.packageSha256,
      )).status,
      "storage_failed",
      "creates PREPARED pending recovery",
    );
    const before = await readJournal(id, featureId);
    assertNoAttempt(before);
    const beforeLifecycle = asRecord(before.lifecycle);
    const beforePending = asRecord(beforeLifecycle.pending);
    assertEquals(
      beforePending.phase,
      "PREPARED",
      "provider failure is injected while the prior selection is still healthy",
    );
    assertEquals(
      beforePending.txId,
      transactionId,
      "original transaction is pending",
    );
    assertEquals(
      beforePending.targetDigest,
      targetMetadata.packageSha256,
      "PREPARED state retains the candidate identity",
    );
    assertEquals(
      beforeLifecycle.selectedDigest,
      oldMetadata.packageSha256,
      "PREPARED state retains the previous selected package",
    );
    const preparedRevision = beforeLifecycle.stateRevision;
    assert(
      typeof preparedRevision === "number",
      "PREPARED journal has a durable revision",
    );
    await host.close();
    host = null;
    const oldFile = cacheFile(id, oldMetadata.packageSha256);
    await IOUtils.remove(oldFile.path);
    oldFile.create(Components.interfaces.nsIFile.DIRECTORY_TYPE!, 0o700);
    const observed: Readonly<Record<string, unknown>>[] = [];
    const hooks: Hooks = {
      afterDurableWriteForTests(snapshot) {
        observed.push(snapshot);
      },
    };
    await assertReopenFacadeRejects(
      () =>
        signedModule.reopenSignedFeatureHostForTests(
          id,
          featureId,
          true,
          authorization,
          hooks,
        ),
      "provider-unsafe-layout",
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      evaluations,
      "provider recovery failure evaluates no candidate",
    );
    const afterFailure = await readJournal(id, featureId);
    const afterFailureLifecycle = asRecord(afterFailure.lifecycle);
    const afterFailurePending = asRecord(afterFailureLifecycle.pending);
    assertEquals(
      afterFailurePending.phase,
      "OLD_STOPPED",
      "provider failure occurs after durable OLD_STOPPED recovery state",
    );
    assertEquals(
      afterFailurePending.txId,
      transactionId,
      "OLD_STOPPED record retains the original transaction",
    );
    assertEquals(
      afterFailurePending.targetDigest,
      oldMetadata.packageSha256,
      "OLD_STOPPED record targets the exact previous package",
    );
    assertEquals(
      afterFailureLifecycle.stateRevision,
      preparedRevision + 1,
      "provider failure follows one durable recovery-state transition",
    );
    assertEquals(
      afterFailure.highWater?.releaseSequence ?? null,
      2,
      "provider recovery failure preserves floor",
    );
    assertEquals(
      afterFailure.highWater?.packageSha256 ?? null,
      targetMetadata.packageSha256,
      "provider recovery failure preserves floor identity",
    );
    assertNoAttempt(afterFailure);
    const oldStoppedObservation = observed.find((snapshot) => {
      const pending = snapshot.pending;
      return isRecord(pending) && pending.phase === "OLD_STOPPED" &&
        pending.txId === transactionId &&
        pending.targetDigest === oldMetadata.packageSha256;
    });
    assert(
      oldStoppedObservation,
      "recovery observer captured OLD_STOPPED for the previous package and original transaction",
    );
    assertEquals(
      oldStoppedObservation.stateRevision,
      afterFailureLifecycle.stateRevision,
      "observed OLD_STOPPED revision is the durable post-failure revision",
    );
    assert(
      observed.every((snapshot) => {
        const attempt = snapshot.attempt;
        if (!isRecord(attempt)) return true;
        const attempted = attempt.attemptedDigests;
        const rejected = attempt.rejectedDigests;
        return Array.isArray(attempted) && Array.isArray(rejected) &&
          attempted.length === 0 && rejected.length === 0;
      }),
      "provider failure consumes no attempted or rejected candidate budget",
    );
    await IOUtils.remove(oldFile.path, { recursive: true });
    await IOUtils.write(oldFile.path, oldPackage.bytes, {
      mode: "create",
    });
    const beforeSuccessfulRecovery = hostModule.m0PayloadEvaluationsForTests();
    reopened = await signedModule.reopenSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    assertEquals(
      String(reopened.diagnostics().selectedPackageSha256),
      oldMetadata.packageSha256,
      "recovery succeeds after owned cache layout is restored",
    );
    assertEquals(
      hostModule.m0PayloadEvaluationsForTests(),
      beforeSuccessfulRecovery + 1,
      "successful recovery evaluates previous selection once",
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

async function testStaleCloseCannotReleaseReopenedOwner(): Promise<void> {
  const id = testId();
  const authority = await createSigningAuthority();
  const authorization = authorizationFor(authority, featureId);
  const fixture = await signedPackage({ authority, featureId });
  let old: Host | null = null;
  let current: Host | null = null;
  let clean = false;
  try {
    old = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    await old.close();
    current = await signedModule.reopenSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    await old.close();
    await assertFacadeRejects(
      Promise.resolve().then(() =>
        signedModule.createSignedFeatureHostForTests(
          id,
          featureId,
          true,
          authorization,
        )
      ),
      "owner-active",
    );
    assertEquals(
      (await current.stage(fixture.bytes)).version,
      "same-display-label",
      "new owner remains usable after stale close",
    );
    await current.close();
    current = null;
    clean = true;
  } finally {
    if (old) await old.close();
    if (current) await current.close();
    if (clean) await removeTestRoots(id);
  }
}

async function testJournalLayoutAndEncodingRejectWithoutRewrite(): Promise<
  void
> {
  const id = testId();
  const authority = await createSigningAuthority();
  const authorization = authorizationFor(authority, featureId);
  let host: Host | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    await host.close();
    host = null;
    const recordFile = journalFile(id, featureId);
    const original = await IOUtils.read(recordFile.path);
    const originalObject = JSON.parse(
      new TextDecoder().decode(original),
    ) as Record<string, unknown>;

    const malformed = encoder.encode("{");
    await IOUtils.write(recordFile.path, malformed);
    await assertFacadeRejects(
      signedModule.reopenSignedFeatureHostForTests(
        id,
        featureId,
        true,
        authorization,
      ),
      "invalid-journal",
    );
    assert(
      bytesEqual(await IOUtils.read(recordFile.path), malformed),
      "malformed JSON is not rewritten",
    );
    await IOUtils.write(recordFile.path, original);

    const badSchema = { ...originalObject, schemaVersion: 2 };
    const badSchemaBytes = encoder.encode(JSON.stringify(badSchema));
    await IOUtils.write(recordFile.path, badSchemaBytes);
    await assertFacadeRejects(
      signedModule.reopenSignedFeatureHostForTests(
        id,
        featureId,
        true,
        authorization,
      ),
      "invalid-journal",
    );
    assert(
      bytesEqual(await IOUtils.read(recordFile.path), badSchemaBytes),
      "invalid schema is not rewritten",
    );
    await IOUtils.write(recordFile.path, original);

    const badNested = JSON.parse(new TextDecoder().decode(original)) as Record<
      string,
      unknown
    >;
    const lifecycle = asRecord(badNested.lifecycle);
    lifecycle.selectedDigest = "a".repeat(64);
    const badNestedBytes = encoder.encode(JSON.stringify(badNested));
    await IOUtils.write(recordFile.path, badNestedBytes);
    await assertFacadeRejects(
      signedModule.reopenSignedFeatureHostForTests(
        id,
        featureId,
        true,
        authorization,
      ),
      "invalid-journal",
    );
    assert(
      bytesEqual(await IOUtils.read(recordFile.path), badNestedBytes),
      "invalid nested references are not rewritten",
    );
    await IOUtils.write(recordFile.path, original);

    const invalidUtf8 = new Uint8Array([0xc3, 0x28]);
    await IOUtils.write(recordFile.path, invalidUtf8);
    await assertFacadeRejects(
      signedModule.reopenSignedFeatureHostForTests(
        id,
        featureId,
        true,
        authorization,
      ),
      "invalid-journal",
    );
    assert(
      bytesEqual(await IOUtils.read(recordFile.path), invalidUtf8),
      "invalid UTF-8 is not rewritten",
    );
    await IOUtils.write(recordFile.path, original);

    const oversized = new Uint8Array(256 * 1024 + 1);
    await IOUtils.write(recordFile.path, oversized);
    await assertFacadeRejects(
      signedModule.reopenSignedFeatureHostForTests(
        id,
        featureId,
        true,
        authorization,
      ),
      "journal-too-large",
    );
    assert(
      bytesEqual(await IOUtils.read(recordFile.path), oversized),
      "oversized journal is not rewritten",
    );
    await IOUtils.write(recordFile.path, original);

    await IOUtils.remove(recordFile.path);
    recordFile.create(Components.interfaces.nsIFile.DIRECTORY_TYPE!, 0o700);
    await assertFacadeRejects(
      signedModule.reopenSignedFeatureHostForTests(
        id,
        featureId,
        true,
        authorization,
      ),
      "invalid-journal",
    );
    assert(recordFile.isDirectory(), "journal directory is preserved");
    await IOUtils.remove(recordFile.path, { recursive: true });
    await IOUtils.write(recordFile.path, original, { mode: "create" });

    const extra = featureRoot(journalRootName, id, featureId);
    extra.append("unrecognized");
    await IOUtils.write(extra.path, new Uint8Array([1, 2, 3]), {
      mode: "create",
    });
    await assertFacadeRejects(
      signedModule.reopenSignedFeatureHostForTests(
        id,
        featureId,
        true,
        authorization,
      ),
      "invalid-journal",
    );
    assert(extra.exists(), "unknown journal entry is preserved");
    assert(
      bytesEqual(await IOUtils.read(recordFile.path), original),
      "unknown entry rejection does not rewrite journal",
    );
    await IOUtils.remove(extra.path);

    host = await signedModule.reopenSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    await host.close();
    host = null;
    clean = true;
  } finally {
    if (host) await host.close();
    if (clean) await removeTestRoots(id);
  }
}

async function testJournalSymlinkRejectsWithoutRewrite(): Promise<void> {
  const id = testId();
  const authority = await createSigningAuthority();
  const authorization = authorizationFor(authority, featureId);
  let host: Host | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    await host.close();
    host = null;
    const recordFile = journalFile(id, featureId);
    const original = await IOUtils.read(recordFile.path);
    const linkTarget = ownedRoot(journalRootName, id);
    linkTarget.append("journal-link-target");
    await IOUtils.write(linkTarget.path, original, { mode: "create" });
    await IOUtils.remove(recordFile.path);
    createSymlink(linkTarget.path, recordFile.path);
    await assertFacadeRejects(
      signedModule.reopenSignedFeatureHostForTests(
        id,
        featureId,
        true,
        authorization,
      ),
      "invalid-journal",
    );
    assert(recordFile.isSymlink(), "journal symlink is preserved");
    assert(
      bytesEqual(await IOUtils.read(linkTarget.path), original),
      "symlink target is unchanged",
    );
    await IOUtils.remove(recordFile.path);
    await IOUtils.remove(linkTarget.path);
    await IOUtils.write(recordFile.path, original, { mode: "create" });
    host = await signedModule.reopenSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    await host.close();
    host = null;
    clean = true;
  } finally {
    if (host) await host.close();
    if (clean) await removeTestRoots(id);
  }
}

async function testLowerSequenceConflictRejectsWithoutRewrite(): Promise<void> {
  const id = testId();
  const authority = await createSigningAuthority();
  const authorization = authorizationFor(authority, featureId);
  const firstPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 1,
    version: "v1",
  });
  const equivocalPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 1,
    version: "v1-alt",
  });
  const currentPackage = await signedPackage({
    authority,
    featureId,
    releaseSequence: 2,
    version: "v2",
  });
  let host: Host | null = null;
  let reopened: Host | null = null;
  let clean = false;
  try {
    host = await signedModule.createSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
    );
    const first = await installAndEnable(host, id, firstPackage.bytes);
    const equivocal = await host.stage(equivocalPackage.bytes);
    const current = await host.stage(currentPackage.bytes);
    const updated = await host.update(
      `lower-sequence-conflict-${id}`,
      revision(host),
      current.packageSha256,
    );
    assertEquals(updated.status, "succeeded", "sequence two is selected");

    const recordFile = journalFile(id, featureId);
    const original = await IOUtils.read(recordFile.path);
    const candidate = JSON.parse(new TextDecoder().decode(original)) as Record<
      string,
      unknown
    >;
    const packages = candidate.packages;
    assert(Array.isArray(packages), "journal packages are an array");
    if (
      !(packages as Record<string, unknown>[]).some((entry) =>
        entry.packageSha256 === equivocal.packageSha256
      )
    ) {
      (packages as Record<string, unknown>[]).push({
        packageSha256: equivocal.packageSha256,
        sourceSha256: equivocal.sourceSha256,
        version: equivocal.version,
        releaseSequence: equivocal.releaseSequence,
      });
    }
    const lifecycle = asRecord(candidate.lifecycle);
    const healthBuild = {
      hostBuildId: "m0-schema-2",
      runtimeBuildId: Services.appinfo.appBuildID,
    };
    lifecycle.selectedVersion = first.packageSha256;
    lifecycle.selectedDigest = first.packageSha256;
    lifecycle.selectedHealth = "verified";
    lifecycle.selectedHealthBuild = healthBuild;
    lifecycle.rollbackVersion = equivocal.packageSha256;
    lifecycle.rollbackDigest = equivocal.packageSha256;
    lifecycle.rollbackHealthBuild = healthBuild;

    const invalid = encoder.encode(JSON.stringify(candidate));
    await host.close();
    host = null;
    await IOUtils.write(recordFile.path, invalid);
    await assertReopenFacadeRejects(
      () =>
        signedModule.reopenSignedFeatureHostForTests(
          id,
          featureId,
          true,
          authorization,
        ),
      "invalid-journal",
    );
    assert(
      bytesEqual(await IOUtils.read(recordFile.path), invalid),
      "lower-sequence conflict is not rewritten",
    );
    await IOUtils.write(recordFile.path, original);
    reopened = await signedModule.reopenSignedFeatureHostForTests(
      id,
      featureId,
      true,
      authorization,
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

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "M3c pre-replace write failure preserves state and retries",
      fn: testBeforeReplacePreservesJournalAndRetries,
    },
    {
      name: "M3c after-replace PREPARED recovery preserves floor",
      fn: testAfterReplacePreparedRestoresPreviousSelection,
    },
    {
      name: "M3c provider unsafe-layout failure blocks without writing",
      fn: () => runProviderPreflightFailure("unsafe"),
    },
    {
      name: "M3c candidate-unavailable normalization is finite and nonblocking",
      fn: testCandidateUnavailableIsFiniteAndNonBlocking,
    },
    {
      name: "M3c stage-only failure does not quarantine",
      fn: testStageOnlyFailureDoesNotQuarantine,
    },
    {
      name:
        "M3c recovery uses only exact eligible fallback and two-candidate ledger",
      fn: async () => {
        await runRecoveryFallback("missing");
        await runRecoveryFallback("corrupt");
        await runRecoveryFallback("policy");
      },
    },
    {
      name: "M3c provider failure during recovery consumes no ledger",
      fn: testProviderRecoveryDoesNotConsumeLedger,
    },
    {
      name: "M3c stale close cannot release reopened owner",
      fn: testStaleCloseCannotReleaseReopenedOwner,
    },
    {
      name: "M3c journal invalid layouts and encodings are not rewritten",
      fn: testJournalLayoutAndEncodingRejectWithoutRewrite,
    },
    {
      name: "M3c journal lower-sequence conflict is rejected on read",
      fn: testLowerSequenceConflictRejectsWithoutRewrite,
    },
  ];
  if (Services.appinfo.OS === "Linux") {
    tests.push({
      name: "M3c journal symlink is rejected without rewrite",
      fn: testJournalSymlinkRejectsWithoutRewrite,
    });
  } else {
    console.info("[SKIP] M3c journal symlink rejection is Linux-only");
  }
  if (await supportsPermissionDeniedCacheIo()) {
    tests.push({
      name: "M3c permission-denied provider I/O blocks without writing",
      fn: () => runProviderPreflightFailure("io"),
    });
  } else {
    console.info(
      "[SKIP] M3c permission-denied provider I/O requires effective non-root Linux permissions",
    );
  }
  await runTests("M3cSignedExecutionStorageFaultAcceptance.test.mts", tests);
}
