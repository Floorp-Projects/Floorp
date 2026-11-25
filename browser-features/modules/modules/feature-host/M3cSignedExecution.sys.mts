// SPDX-License-Identifier: MPL-2.0

import {
  createM0EmptyRecordForSignedTests,
  M0FeatureHost,
  type M0FeatureHostTestOptions,
  M0ProviderInfrastructureFailure,
  m0SignedCapabilitiesAllowedForTests,
  type M0SignedHostOpenOptions,
  m0SignedPolicyBindingForTests,
  retireM0FeatureHostOwnerAfterSignedTests,
} from "./FeatureHost.sys.mts";
import type { M0PackageProvider } from "./M0FeatureHostAdapters.types.ts";
import { M3cIdentityCatalog, M3cJournalError } from "./M3cSignedJournal.ts";
import type { M3cPackageMetadata } from "./M3cSignedJournal.types.ts";
import { classifyM3cProviderFailure } from "./M3cProviderTaxonomy.ts";
import { M3cSerialQueue } from "./M3cSerialQueue.ts";
import { M3cSignedJournalStore } from "./M3cSignedJournalStore.sys.mts";
import { isHubIdentifier } from "./HubPageDescriptorValidation.ts";
import {
  FeaturePackageCacheError,
  openTestProfileFeaturePackageCache,
} from "./package/FeaturePackageCache.sys.mts";
import {
  FeaturePackageError,
  isValidFeaturePackageId,
  snapshotFeaturePackageBytes,
  snapshotFeaturePackagePolicy,
} from "./package/FeaturePackageVerifier.ts";
import type {
  FeaturePackageCache,
  FeaturePackagePolicy,
  VerifiedFeaturePackage,
} from "./package/types.ts";

const testIdPattern =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;
const hashPattern = /^[a-f0-9]{64}$/;
const owners = new Map<string, symbol>();
const safeEngineErrors = new Set([
  "invalid transaction ID",
  "invalid expected revision",
  "invalid transaction descriptor",
  "transaction ID payload conflict",
  "stale_revision",
  "busy",
  "not installed",
  "already disabled",
  "already enabled",
  "invalid install",
  "missing target version",
  "rollback target is already selected",
  "rollback target lacks current-build health proof",
  "recovery required before new transaction",
  "quarantined",
  "package-unavailable",
  "M0 host closed",
  "M0 host shutdown",
  "host shutdown",
  "storage_failed",
  "window cleanup failed before activation finalized",
  "signed-test-recovery-completed",
]);
const safeFacadeCodes = new Set([
  "invalid-test-id",
  "invalid-feature-id",
  "experimental-opt-in-required",
  "dev-test-only",
  "owner-active",
  "root-exists",
  "journal-missing",
  "unsafe-layout",
  "invalid-authorization",
  "invalid-policy",
  "invalid-test-hook",
  "invalid-transaction-id",
  "invalid-package-hash",
  "invalid-expected-revision",
  "invalid-fault-mode",
  "invalid-fault-offset",
  "invalid-cleanup",
  "cleanup-unproven",
  "stale-owner",
  "invalid-journal",
  "journal-too-large",
  "catalog-full",
  "package-conflict",
  "sequence-too-low",
  "sequence-equivocation",
  "package-unavailable",
  "invalid-envelope",
  "invalid-metadata",
  "invalid-source",
  "invalid-signing-key",
  "unsupported-crypto",
  "unsupported-version",
  "untrusted-key",
  "invalid-signature",
  "source-digest-mismatch",
  "feature-mismatch",
  "host-incompatible",
  "runtime-incompatible",
  "capability-denied",
  "size-limit",
  "invalid-window-id",
  "operation-failed",
  ...safeEngineErrors,
  "provider-cache-io",
  "provider-unsafe-layout",
  "provider-cache-contract",
  "provider-unsupported-crypto",
  "provider-unexpected",
]);
const providerReasons = new Set([
  "cache-io",
  "unsafe-layout",
  "cache-contract",
  "unsupported-crypto",
  "unexpected",
]);

export class M3cSignedFacadeError extends Error {
  constructor(readonly code: string) {
    super(code);
    this.name = "M3cSignedFacadeError";
  }
}

export interface M3cSignedAuthorization {
  readonly trustKind: "core-system-principal";
  readonly policy: FeaturePackagePolicy;
}

export interface M3cSignedTestHooks {
  readonly excludeWindowsForTests?: boolean;
  readonly failInitialJournalForTests?: boolean;
  readonly afterDurableWriteForTests?: (
    snapshot: Readonly<Record<string, unknown>>,
  ) => void | Promise<void>;
  readonly failAfterRecoveryForTests?: boolean;
}

export type M3cSignedOutcome = Awaited<
  ReturnType<M0FeatureHost["install"]>
>;

export interface M3cSignedFeatureHost {
  readonly featureId: string;
  stage(bytes: Uint8Array): Promise<M3cPackageMetadata>;
  install(
    txId: string,
    expectedRevision: number,
    packageSha256: string,
  ): Promise<M3cSignedOutcome>;
  update(
    txId: string,
    expectedRevision: number,
    packageSha256: string,
  ): Promise<M3cSignedOutcome>;
  enable(txId: string, expectedRevision: number): Promise<M3cSignedOutcome>;
  disable(txId: string, expectedRevision: number): Promise<M3cSignedOutcome>;
  rollback(txId: string, expectedRevision: number): Promise<M3cSignedOutcome>;
  queryOutcome(txId: string): M3cSignedOutcome | "outcome_unknown_or_expired";
  metadata(): readonly M3cPackageMetadata[];
  diagnostics(): Readonly<Record<string, unknown>>;
  cleanupDiagnosticsForTests(): Readonly<{
    proven: boolean;
    evidence: readonly string[];
  }>;
  windowIdsForTests(): readonly string[];
  setNextWriteFaultForTests(
    mode: "before_replace" | "after_replace",
    skip?: number,
  ): Promise<void>;
  registerWindowCleanupForTests(
    windowId: string,
    cleanup: () => void | Promise<void>,
  ): Promise<void>;
  close(): Promise<void>;
}

function child(parent: nsIFile, name: string): nsIFile {
  const file = parent.clone();
  file.append(name);
  return file;
}

function assertCacheRootAbsent(testId: string, featureId: string): void {
  const profile = Services.dirsvc.get(
    "ProfD",
    Components.interfaces.nsIFile,
  );
  if (profile.isSymlink() || !profile.isDirectory()) {
    throw new M3cSignedFacadeError("unsafe-layout");
  }
  const namespace = child(profile, "floorp-signed-package-cache-v1");
  const tests = child(namespace, "tests");
  const testRoot = child(tests, testId);
  const root = child(testRoot, featureId);
  for (const directory of [namespace, tests, testRoot]) {
    if (
      directory.exists() &&
      (directory.isSymlink() || !directory.isDirectory())
    ) throw new M3cSignedFacadeError("unsafe-layout");
  }
  if (root.exists()) throw new M3cSignedFacadeError("root-exists");
}

function cacheRootPath(testId: string, featureId: string): string {
  return PathUtils.join(
    PathUtils.profileDir,
    "floorp-signed-package-cache-v1",
    "tests",
    testId,
    featureId,
  );
}

async function removeCreatedDirectories(
  directories: readonly nsIFile[],
  featureLeaves: ReadonlySet<string>,
): Promise<boolean> {
  for (const directory of [...directories].reverse()) {
    if (!directory.exists()) continue;
    if (directory.isSymlink() || !directory.isDirectory()) return false;
    const entries = directory.directoryEntries;
    const hasEntries = entries.nextFile !== null;
    entries.close();
    if (hasEntries) {
      if (featureLeaves.has(directory.path)) return false;
      continue;
    }
    try {
      await IOUtils.remove(directory.path);
    } catch {
      return false;
    }
  }
  return true;
}

function snapshotTestHooks(
  input: M3cSignedTestHooks | undefined,
): Readonly<{
  excludeWindows: boolean;
  failInitialJournal: boolean;
  afterWrite: M3cSignedTestHooks["afterDurableWriteForTests"] | undefined;
  failAfterRecovery: boolean;
}> {
  if (input === undefined) {
    return Object.freeze({
      excludeWindows: false,
      failInitialJournal: false,
      afterWrite: undefined,
      failAfterRecovery: false,
    });
  }
  if (typeof input !== "object" || input === null) {
    throw new M3cSignedFacadeError("invalid-test-hook");
  }
  const excludeWindows = input.excludeWindowsForTests;
  const failInitialJournal = input.failInitialJournalForTests;
  const afterWrite = input.afterDurableWriteForTests;
  const failAfterRecovery = input.failAfterRecoveryForTests;
  if (
    (excludeWindows !== undefined && typeof excludeWindows !== "boolean") ||
    (failInitialJournal !== undefined &&
      typeof failInitialJournal !== "boolean") ||
    (afterWrite !== undefined && typeof afterWrite !== "function") ||
    (failAfterRecovery !== undefined &&
      typeof failAfterRecovery !== "boolean")
  ) throw new M3cSignedFacadeError("invalid-test-hook");
  return Object.freeze({
    excludeWindows: excludeWindows === true,
    failInitialJournal: failInitialJournal === true,
    afterWrite,
    failAfterRecovery: failAfterRecovery === true,
  });
}

function snapshotPolicy(
  authorizationInput: unknown,
  featureId: string,
): FeaturePackagePolicy {
  if (
    typeof authorizationInput !== "object" ||
    authorizationInput === null
  ) throw new M3cSignedFacadeError("invalid-authorization");
  const authorization = authorizationInput as {
    readonly trustKind?: unknown;
    readonly policy?: unknown;
  };
  if (authorization.trustKind !== "core-system-principal") {
    throw new M3cSignedFacadeError("invalid-authorization");
  }
  const policyInput = authorization.policy;
  if (typeof policyInput !== "object" || policyInput === null) {
    throw new M3cSignedFacadeError("invalid-policy");
  }
  const source = policyInput as {
    readonly featureId?: unknown;
    readonly allowedCapabilities?: unknown;
    readonly minimumReleaseSequence?: unknown;
    readonly trustedKeys?: unknown;
  };
  const sourceFeatureId = source.featureId;
  const sourceCapabilities = source.allowedCapabilities;
  const minimumReleaseSequence = source.minimumReleaseSequence;
  const sourceKeys = source.trustedKeys;
  if (
    sourceFeatureId !== featureId || !Array.isArray(sourceCapabilities) ||
    !Array.isArray(sourceKeys)
  ) throw new M3cSignedFacadeError("invalid-policy");
  const binding = m0SignedPolicyBindingForTests();
  let policy: FeaturePackagePolicy;
  try {
    policy = snapshotFeaturePackagePolicy({
      featureId,
      hostSchema: binding.hostSchema,
      hostBuildId: binding.hostBuildId,
      runtimeBuildId: binding.runtimeBuildId,
      allowedCapabilities: sourceCapabilities,
      minimumReleaseSequence,
      trustedKeys: sourceKeys,
    } as FeaturePackagePolicy);
  } catch {
    throw new M3cSignedFacadeError("invalid-policy");
  }
  if (
    !m0SignedCapabilitiesAllowedForTests(featureId, policy.allowedCapabilities)
  ) throw new M3cSignedFacadeError("invalid-policy");
  return policy;
}

function policyFingerprint(policy: FeaturePackagePolicy): string {
  return JSON.stringify({
    featureId: policy.featureId,
    hostSchema: policy.hostSchema,
    hostBuildId: policy.hostBuildId,
    runtimeBuildId: policy.runtimeBuildId,
    allowedCapabilities: [...policy.allowedCapabilities].sort(),
    minimumReleaseSequence: policy.minimumReleaseSequence,
    trustedKeys: [...policy.trustedKeys].sort((left, right) =>
      left.keyId.localeCompare(right.keyId)
    ),
  });
}

function metadataFromVerified(
  verified: VerifiedFeaturePackage,
): M3cPackageMetadata {
  return Object.freeze({
    packageSha256: verified.packageSha256,
    sourceSha256: verified.manifest.sourceSha256,
    version: verified.manifest.version,
    releaseSequence: verified.manifest.releaseSequence,
  });
}

class CandidateUnavailable extends Error {}

function providerFailure(error: unknown): Error {
  let code = "unexpected";
  try {
    if (error instanceof FeaturePackageCacheError) {
      code = error.code;
    }
    if (error instanceof FeaturePackageError) code = error.code;
  } catch {
    code = "unexpected";
  }
  const result = classifyM3cProviderFailure(code);
  return result.kind === "candidate-unavailable"
    ? new CandidateUnavailable("package-unavailable")
    : new M0ProviderInfrastructureFailure(result.reason);
}

class M3cSignedPackageProvider implements M0PackageProvider {
  readonly #cache: FeaturePackageCache;
  readonly #policy: FeaturePackagePolicy;
  readonly #catalog: M3cIdentityCatalog;
  readonly #featureId: string;

  constructor(
    cache: FeaturePackageCache,
    policy: FeaturePackagePolicy,
    catalog: M3cIdentityCatalog,
    featureId: string,
  ) {
    this.#cache = cache;
    this.#policy = policy;
    this.#catalog = catalog;
    this.#featureId = featureId;
  }

  async ensure(version: string) {
    return await this.#load(version, version);
  }

  async load(digest: string, version: string) {
    return await this.#load(digest, version);
  }

  async #load(digest: string, version: string) {
    const metadata = this.#catalog.get(version);
    if (
      !metadata || digest !== version ||
      digest !== metadata.packageSha256
    ) throw new CandidateUnavailable("package-unavailable");
    let verified: VerifiedFeaturePackage;
    try {
      verified = await this.#cache.read(digest, this.#policy);
    } catch (error) {
      throw providerFailure(error);
    }
    if (
      verified.packageSha256 !== metadata.packageSha256 ||
      verified.manifest.featureId !== this.#featureId ||
      verified.manifest.sourceSha256 !== metadata.sourceSha256 ||
      verified.manifest.version !== metadata.version ||
      verified.manifest.releaseSequence !== metadata.releaseSequence ||
      !m0SignedCapabilitiesAllowedForTests(
        this.#featureId,
        verified.manifest.compatibility.capabilities,
      )
    ) throw new CandidateUnavailable("package-unavailable");
    return Object.freeze({
      version: metadata.packageSha256,
      digest: metadata.packageSha256,
      source: verified.source,
      compatibility: Object.freeze({
        hostSchema: verified.manifest.compatibility.hostSchema,
        hostBuildId: verified.manifest.compatibility.hostBuildId,
        runtimeBuildIds: verified.manifest.compatibility.runtimeBuildIds,
        capabilities: verified.manifest.compatibility.capabilities,
      }),
    });
  }
}

function safeError(error: unknown): M3cSignedFacadeError {
  try {
    if (error instanceof M3cSignedFacadeError) {
      const code = error.code;
      return new M3cSignedFacadeError(
        typeof code === "string" && safeFacadeCodes.has(code)
          ? code
          : "operation-failed",
      );
    }
    if (error instanceof M3cJournalError) {
      const code = error.code;
      return new M3cSignedFacadeError(
        typeof code === "string" && safeFacadeCodes.has(code)
          ? code
          : "operation-failed",
      );
    }
    if (error instanceof FeaturePackageCacheError) {
      const classified = classifyM3cProviderFailure(error.code);
      return new M3cSignedFacadeError(
        classified.kind === "candidate-unavailable"
          ? "package-unavailable"
          : "provider-" + classified.reason,
      );
    }
    if (error instanceof FeaturePackageError) {
      const classified = classifyM3cProviderFailure(error.code);
      return new M3cSignedFacadeError(
        classified.kind === "candidate-unavailable"
          ? "package-unavailable"
          : "provider-" + classified.reason,
      );
    }
    if (error instanceof M0ProviderInfrastructureFailure) {
      const reason = error.reason;
      return new M3cSignedFacadeError(
        typeof reason === "string" && providerReasons.has(reason)
          ? "provider-" + reason
          : "operation-failed",
      );
    }
    if (typeof error === "object" && error !== null && "message" in error) {
      const message = (error as { message?: unknown }).message;
      if (typeof message === "string" && safeEngineErrors.has(message)) {
        return new M3cSignedFacadeError(message);
      }
    }
  } catch {
    return new M3cSignedFacadeError("operation-failed");
  }
  return new M3cSignedFacadeError("operation-failed");
}

function assertString(value: unknown, code: string): string {
  if (typeof value !== "string") throw new M3cSignedFacadeError(code);
  return value;
}

function assertRevision(value: unknown): number {
  if (
    typeof value !== "number" || !Number.isSafeInteger(value) || value < 0
  ) throw new M3cSignedFacadeError("invalid-expected-revision");
  return value;
}

function detached<T>(value: T): T {
  return JSON.parse(JSON.stringify(value)) as T;
}

function createFacade(
  featureId: string,
  ownerKey: string,
  ownerToken: symbol,
  policy: FeaturePackagePolicy,
  engine: M0FeatureHost,
  cache: FeaturePackageCache,
  catalog: M3cIdentityCatalog,
): M3cSignedFeatureHost {
  const queue = new M3cSerialQueue(16);
  const metadataFor = (token: string | null): M3cPackageMetadata | null =>
    token ? catalog.get(token) : null;
  function schedule<T>(
    prepare: () => () => Promise<T>,
  ): Promise<T> {
    return queue.schedule(prepare).catch((error: unknown) => {
      throw safeError(error);
    });
  }
  const facade: M3cSignedFeatureHost = {
    featureId,
    stage(bytes) {
      return schedule(() => {
        const snapshot = snapshotFeaturePackageBytes(bytes);
        return async () => {
          let verified: VerifiedFeaturePackage;
          try {
            verified = await cache.put(snapshot, policy);
          } catch (error) {
            throw safeError(error);
          }
          const metadata = metadataFromVerified(verified);
          catalog.add(metadata);
          return detached(metadata);
        };
      });
    },
    install(txId, expectedRevision, packageSha256) {
      return schedule(() => {
        const id = assertString(txId, "invalid-transaction-id");
        const revision = assertRevision(expectedRevision);
        const hash = assertString(packageSha256, "invalid-package-hash");
        if (!hashPattern.test(hash)) {
          throw new M3cSignedFacadeError("invalid-package-hash");
        }
        return async () =>
          detached(
            await engine.install(id, revision, hash),
          );
      });
    },
    update(txId, expectedRevision, packageSha256) {
      return schedule(() => {
        const id = assertString(txId, "invalid-transaction-id");
        const revision = assertRevision(expectedRevision);
        const hash = assertString(packageSha256, "invalid-package-hash");
        if (!hashPattern.test(hash)) {
          throw new M3cSignedFacadeError("invalid-package-hash");
        }
        return async () =>
          detached(
            await engine.update(id, revision, hash),
          );
      });
    },
    enable(txId, expectedRevision) {
      return schedule(() => {
        const id = assertString(txId, "invalid-transaction-id");
        const revision = assertRevision(expectedRevision);
        return async () => detached(await engine.enable(id, revision));
      });
    },
    disable(txId, expectedRevision) {
      return schedule(() => {
        const id = assertString(txId, "invalid-transaction-id");
        const revision = assertRevision(expectedRevision);
        return async () => detached(await engine.disable(id, revision));
      });
    },
    rollback(txId, expectedRevision) {
      return schedule(() => {
        const id = assertString(txId, "invalid-transaction-id");
        const revision = assertRevision(expectedRevision);
        return async () => detached(await engine.rollback(id, revision));
      });
    },
    queryOutcome(txId) {
      const id = assertString(txId, "invalid-transaction-id");
      try {
        return detached(engine.queryOutcome(id));
      } catch (error) {
        throw safeError(error);
      }
    },
    metadata() {
      return detached(catalog.values());
    },
    diagnostics() {
      const state = engine.signedStateForTests();
      return detached({
        featureId,
        revision: state.revision,
        desiredEnabled: state.desiredEnabled,
        selected: metadataFor(state.selectedVersion),
        selectedPackageSha256: state.selectedDigest,
        selectedHealth: state.selectedHealth,
        rollback: metadataFor(state.rollbackVersion),
        blocked: state.blocked,
        blockCategory: state.providerBlockReason ??
          (state.blocked ? "lifecycle-blocked" : null),
      });
    },
    cleanupDiagnosticsForTests() {
      const result = engine.quiescenceForSignedTests();
      return Object.freeze({
        proven: result.proven,
        evidence: Object.freeze([...result.evidence]),
      });
    },
    windowIdsForTests() {
      const windowIds = engine.windowIdsForTests;
      if (windowIds.length > 64) {
        throw new M3cSignedFacadeError("window-observation-limit");
      }
      return Object.freeze([...windowIds]);
    },
    setNextWriteFaultForTests(mode, skip = 0) {
      return schedule(() => {
        if (mode !== "before_replace" && mode !== "after_replace") {
          throw new M3cSignedFacadeError("invalid-fault-mode");
        }
        if (!Number.isInteger(skip) || skip < 0 || skip > 32) {
          throw new M3cSignedFacadeError("invalid-fault-offset");
        }
        return () =>
          Promise.resolve().then(() => {
            engine.setNextWriteFaultForTests(mode, skip);
          });
      });
    },
    registerWindowCleanupForTests(windowId, cleanup) {
      return schedule(() => {
        const id = assertString(windowId, "invalid-window-id");
        if (typeof cleanup !== "function") {
          throw new M3cSignedFacadeError("invalid-cleanup");
        }
        return () =>
          Promise.resolve().then(() =>
            engine.registerWindowCleanupForTests(id, cleanup)
          );
      });
    },
    close() {
      return queue.close(async () => {
        try {
          const report = await engine.shutdown();
          let cacheCloseError: unknown = null;
          try {
            await cache.close();
          } catch (error) {
            cacheCloseError = error;
          }
          if (
            report.errors.length !== 0 || report.timedOut ||
            !engine.quiescenceForSignedTests().proven ||
            cacheCloseError !== null
          ) throw new M3cSignedFacadeError("cleanup-unproven");
          if (!retireM0FeatureHostOwnerAfterSignedTests(engine)) {
            throw new M3cSignedFacadeError("cleanup-unproven");
          }
          if (owners.get(ownerKey) !== ownerToken) {
            throw new M3cSignedFacadeError("stale-owner");
          }
          owners.delete(ownerKey);
        } catch (error) {
          throw safeError(error);
        }
      });
    },
  };
  return Object.freeze(facade);
}

function openSigned(
  mode: "create" | "reopen",
  testIdInput: unknown,
  featureIdInput: unknown,
  experimentalOptIn: unknown,
  authorization: unknown,
  hooksInput?: M3cSignedTestHooks,
): Promise<M3cSignedFeatureHost> {
  let startupMode: string;
  try {
    startupMode = Services.prefs.getStringPref("nora.startup.mode", "");
  } catch {
    throw new M3cSignedFacadeError("dev-test-only");
  }
  if (startupMode !== "dev" && startupMode !== "test") {
    throw new M3cSignedFacadeError("dev-test-only");
  }
  if (
    typeof testIdInput !== "string" || !testIdPattern.test(testIdInput)
  ) throw new M3cSignedFacadeError("invalid-test-id");
  if (
    typeof featureIdInput !== "string" ||
    !isHubIdentifier(featureIdInput) ||
    !isValidFeaturePackageId(featureIdInput)
  ) throw new M3cSignedFacadeError("invalid-feature-id");
  if (experimentalOptIn !== true) {
    throw new M3cSignedFacadeError("experimental-opt-in-required");
  }
  const testId = testIdInput;
  const featureId = featureIdInput;
  const rootPath = PathUtils.join(
    PathUtils.profileDir,
    "floorp-hotfix-signed-tests-v1",
    testId,
    featureId,
  );
  const ownerKey = rootPath;
  if (owners.has(ownerKey)) throw new M3cSignedFacadeError("owner-active");
  const ownerToken = Symbol("m3c-signed-owner");
  owners.set(ownerKey, ownerToken);
  return (async () => {
    let cache: FeaturePackageCache | null = null;
    let engine: M0FeatureHost | null = null;
    let store: M3cSignedJournalStore | null = null;
    let clean = true;
    const createdCacheDirectories: nsIFile[] = [];
    try {
      const policy = snapshotPolicy(authorization, featureId);
      const hooks = snapshotTestHooks(hooksInput);
      const catalog = new M3cIdentityCatalog(featureId);
      store = new M3cSignedJournalStore(testId, featureId, catalog);
      if (mode === "create") {
        if (store.rootExists()) throw new M3cSignedFacadeError("root-exists");
        assertCacheRootAbsent(testId, featureId);
      } else if (!store.rootExists()) {
        throw new M3cSignedFacadeError("journal-missing");
      }
      let testOptions: M0FeatureHostTestOptions | undefined;
      if (hooks.afterWrite !== undefined || hooks.excludeWindows) {
        testOptions = {};
      }
      if (hooks.excludeWindows) {
        testOptions = { ...testOptions, excludeWindowsForTests: true };
      }
      if (hooks.afterWrite !== undefined) {
        testOptions = {
          ...testOptions,
          afterDurableWriteForTests: async (snapshot) => {
            await hooks.afterWrite?.(snapshot);
          },
        };
      }
      cache = await openTestProfileFeaturePackageCache(
        featureId,
        testId,
        (directory) => createdCacheDirectories.push(directory),
      );
      if (mode === "create") {
        await store.createEmpty(
          createM0EmptyRecordForSignedTests(featureId),
          hooks.failInitialJournal,
        );
      }
      const packageProvider = new M3cSignedPackageProvider(
        cache,
        policy,
        catalog,
        featureId,
      );
      const options: M0SignedHostOpenOptions = {
        testId,
        definitionFingerprint: policyFingerprint(policy),
        identityCatalog: catalog,
        packageProvider,
        recordStore: store,
        testOptions,
        onEngineCreated: (created) => engine = created,
        failAfterRecoveryForTests: hooks.failAfterRecovery,
      };
      engine = await M0FeatureHost.openSignedForTests(
        rootPath,
        true,
        options,
      );
      return createFacade(
        featureId,
        ownerKey,
        ownerToken,
        policy,
        engine,
        cache,
        catalog,
      );
    } catch (error) {
      let engineClean = false;
      let cacheClosed = true;
      if (engine) {
        try {
          const report = await engine.shutdown();
          engineClean = report.errors.length === 0 && !report.timedOut &&
            engine.quiescenceForSignedTests().proven;
          clean = engineClean;
        } catch {
          clean = false;
        }
      }
      if (cache) {
        try {
          await cache.close();
        } catch {
          cacheClosed = false;
          clean = false;
        }
      }
      let innerOwnerRetired = engine === null;
      if (engine && engineClean) {
        innerOwnerRetired = retireM0FeatureHostOwnerAfterSignedTests(engine);
        clean &&= innerOwnerRetired;
      }
      if (mode === "create" && engine === null && cacheClosed) {
        try {
          const journalClean = store
            ? await store.cleanupPartialCreate()
            : true;
          if (journalClean) {
            clean &&= await removeCreatedDirectories(
              createdCacheDirectories,
              new Set([cacheRootPath(testId, featureId)]),
            );
          } else {
            clean = false;
          }
        } catch {
          clean = false;
        }
      }
      if (clean && innerOwnerRetired && owners.get(ownerKey) === ownerToken) {
        owners.delete(ownerKey);
      }
      throw safeError(error);
    }
  })();
}

export function createSignedFeatureHostForTests(
  testId: string,
  featureId: string,
  experimentalOptIn: boolean,
  authorization: M3cSignedAuthorization,
  hooks?: M3cSignedTestHooks,
): Promise<M3cSignedFeatureHost> {
  return openSigned(
    "create",
    testId,
    featureId,
    experimentalOptIn,
    authorization,
    hooks,
  );
}

export function reopenSignedFeatureHostForTests(
  testId: string,
  featureId: string,
  experimentalOptIn: boolean,
  authorization: M3cSignedAuthorization,
  hooks?: M3cSignedTestHooks,
): Promise<M3cSignedFeatureHost> {
  return openSigned(
    "reopen",
    testId,
    featureId,
    experimentalOptIn,
    authorization,
    hooks,
  );
}
