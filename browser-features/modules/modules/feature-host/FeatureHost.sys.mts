// SPDX-License-Identifier: MPL-2.0

import { type FixtureVersion, M0_FIXTURES } from "./M0Fixtures.sys.mts";
import {
  type DisposeReport,
  type M0Lifecycle,
  M0ModuleHost,
} from "./M0ModuleHost.sys.mts";

const FEATURE_ID = "experimental-local-sample";
const HOST_BUILD_ID = "m0-schema-1";
const ROOT = PathUtils.join(PathUtils.profileDir, "floorp-hotfix-m0");
const TEST_ROOT = PathUtils.join(
  PathUtils.profileDir,
  "floorp-hotfix-m0-tests",
);
const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
);

type Operation = "install" | "enable" | "disable" | "update" | "rollback";
type OutcomeStatus =
  | "pending"
  | "succeeded"
  | "failed_restored"
  | "failed_quarantined"
  | "storage_failed";

interface Descriptor {
  featureId: string;
  op: Operation;
  expectedRevision: number;
  target: FixtureVersion | null;
  normalizedInput: string;
}

interface Outcome {
  txId: string;
  request: Descriptor;
  status: OutcomeStatus;
  selectedVersion: FixtureVersion | null;
}

interface M0Record {
  schemaVersion: 1;
  featureId: string;
  desiredEnabled: boolean;
  selectedVersion: FixtureVersion | null;
  selectedDigest: string | null;
  selectedHealth: "verified" | "unverified";
  selectedHealthBuild: { hostBuildId: string; runtimeBuildId: string } | null;
  rollbackVersion: FixtureVersion | null;
  rollbackDigest: string | null;
  rollbackHealthBuild: { hostBuildId: string; runtimeBuildId: string } | null;
  stateRevision: number;
  pending: { txId: string; request: Descriptor; phase: string } | null;
  startupAttempt: null;
  recoveryAttempt: null;
  blockedReason: string | null;
  recentOutcomes: Outcome[];
  lastOutcome: Outcome | null;
}

interface WindowEntry {
  id: string;
  window: Window;
}

const owners = new Map<string, M0FeatureHost>();
const openingHosts = new Map<string, Promise<M0FeatureHost>>();

export function isEligibleM0MainWindowForTests(
  windowType: string | null,
  toolbarVisible: boolean,
  isPrivate: boolean,
  isWebPanel: boolean,
): boolean {
  return windowType === "navigator:browser" && toolbarVisible && !isPrivate &&
    !isWebPanel;
}

export function hasUnfinishedM0FinalizationForTests(
  pendingTxId: string | null,
  terminalTxId: string | null,
  terminalStatus: string | null,
  txId: string,
): boolean {
  if (terminalTxId === txId) return terminalStatus === "pending";
  return pendingTxId === txId;
}

function detachedOutcome(outcome: Outcome): Outcome {
  return JSON.parse(JSON.stringify(outcome)) as Outcome;
}

function isM0WebPanelWindow(window: Window): boolean {
  if (
    (window as Window & { floorpWebPanelWindow?: boolean }).floorpWebPanelWindow
  ) {
    return true;
  }
  try {
    return new URL(window.location.href).searchParams.has("floorpWebPanelId");
  } catch {
    return true;
  }
}

function isEligibleM0Window(window: Window): boolean {
  if (window.closed) return false;
  const documentElement = window.document.documentElement;
  const toolbarVisible = (window as Window & {
    toolbar?: { visible?: boolean };
  }).toolbar?.visible === true;
  return isEligibleM0MainWindowForTests(
    documentElement?.getAttribute("windowtype") ?? null,
    toolbarVisible,
    PrivateBrowsingUtils.isWindowPrivate(window),
    isM0WebPanelWindow(window),
  );
}

class StorageFailure extends Error {
  constructor() {
    super("storage_failed");
    this.name = "StorageFailure";
  }
}

function defaultRecord(): M0Record {
  return {
    schemaVersion: 1,
    featureId: FEATURE_ID,
    desiredEnabled: false,
    selectedVersion: null,
    selectedDigest: null,
    selectedHealth: "unverified",
    selectedHealthBuild: null,
    rollbackVersion: null,
    rollbackDigest: null,
    rollbackHealthBuild: null,
    stateRevision: 0,
    pending: null,
    startupAttempt: null,
    recoveryAttempt: null,
    blockedReason: null,
    recentOutcomes: [],
    lastOutcome: null,
  };
}

function isVersion(value: unknown): value is FixtureVersion {
  return value === "v1" || value === "v2" || value === "broken";
}

function isDescriptor(value: unknown): value is Descriptor {
  if (typeof value !== "object" || value === null) return false;
  const descriptor = value as Partial<Descriptor>;
  return descriptor.featureId === FEATURE_ID &&
    (descriptor.op === "install" || descriptor.op === "enable" ||
      descriptor.op === "disable" || descriptor.op === "update" ||
      descriptor.op === "rollback") &&
    Number.isInteger(descriptor.expectedRevision) &&
    (descriptor.target === null || isVersion(descriptor.target)) &&
    typeof descriptor.normalizedInput === "string";
}

function isOutcome(value: unknown): value is Outcome {
  if (typeof value !== "object" || value === null) return false;
  const outcome = value as Partial<Outcome>;
  return typeof outcome.txId === "string" && isDescriptor(outcome.request) &&
    (outcome.status === "pending" || outcome.status === "succeeded" ||
      outcome.status === "failed_restored" ||
      outcome.status === "failed_quarantined" ||
      outcome.status === "storage_failed") &&
    (outcome.selectedVersion === null || isVersion(outcome.selectedVersion));
}

function isHealthBuild(
  value: unknown,
): value is M0Record["selectedHealthBuild"] {
  if (value === null) return true;
  if (typeof value !== "object" || value === null) return false;
  const build = value as { hostBuildId?: unknown; runtimeBuildId?: unknown };
  return typeof build.hostBuildId === "string" &&
    typeof build.runtimeBuildId === "string";
}

function validateRecord(value: unknown): M0Record {
  if (typeof value !== "object" || value === null) {
    throw new Error("invalid M0 state record");
  }
  const record = value as Partial<M0Record>;
  if (
    record.schemaVersion !== 1 || record.featureId !== FEATURE_ID ||
    typeof record.desiredEnabled !== "boolean" ||
    !Number.isInteger(record.stateRevision) ||
    (record.stateRevision ?? -1) < 0 ||
    !(record.selectedVersion === null || isVersion(record.selectedVersion)) ||
    !(record.rollbackVersion === null || isVersion(record.rollbackVersion)) ||
    !(record.selectedDigest === null ||
      typeof record.selectedDigest === "string") ||
    !(record.rollbackDigest === null ||
      typeof record.rollbackDigest === "string") ||
    (record.selectedHealth !== "verified" &&
      record.selectedHealth !== "unverified") ||
    !isHealthBuild(record.selectedHealthBuild) ||
    !isHealthBuild(record.rollbackHealthBuild) ||
    (record.selectedHealth === "verified") !==
      (record.selectedHealthBuild !== null) ||
    (record.rollbackVersion === null) !==
      (record.rollbackHealthBuild === null) ||
    !Array.isArray(record.recentOutcomes) ||
    record.recentOutcomes.length > 32 ||
    !record.recentOutcomes.every(isOutcome) ||
    !(record.lastOutcome === null || isOutcome(record.lastOutcome)) ||
    !(record.pending === null || (typeof record.pending === "object" &&
      record.pending !== null && typeof record.pending.txId === "string" &&
      isDescriptor(record.pending.request) &&
      record.pending.phase === "PREPARED")) ||
    record.startupAttempt !== null || record.recoveryAttempt !== null ||
    !(record.blockedReason === null ||
      typeof record.blockedReason === "string") ||
    (record.selectedVersion === null) !== (record.selectedDigest === null) ||
    (record.rollbackVersion === null) !== (record.rollbackDigest === null) ||
    (record.selectedVersion !== null &&
      record.selectedDigest !== M0_FIXTURES[record.selectedVersion].digest) ||
    (record.rollbackVersion !== null &&
      record.rollbackDigest !== M0_FIXTURES[record.rollbackVersion].digest) ||
    (record.desiredEnabled && record.selectedVersion === null)
  ) throw new Error("invalid M0 state record");
  return record as M0Record;
}

async function digest(source: string): Promise<string> {
  const bytes = new TextEncoder().encode(source);
  const data = new ArrayBuffer(bytes.byteLength);
  new Uint8Array(data).set(bytes);
  const result = await crypto.subtle.digest("SHA-256", data);
  return [...new Uint8Array(result)].map((byte) =>
    byte.toString(16).padStart(2, "0")
  ).join("");
}

class PackageStore {
  readonly #directory: string;

  constructor(root: string) {
    this.#directory = PathUtils.join(root, "packages");
  }

  async ensure(version: FixtureVersion): Promise<M0FixtureValue> {
    const fixture = M0_FIXTURES[version];
    if (await digest(fixture.source) !== fixture.digest) {
      throw new Error("pinned fixture digest mismatch");
    }
    try {
      await IOUtils.makeDirectory(this.#directory, {
        createAncestors: true,
        ignoreExisting: true,
        permissions: 0o700,
      });
    } catch {
      throw new StorageFailure();
    }
    const path = PathUtils.join(this.#directory, `${fixture.digest}.js`);
    let exists: boolean;
    try {
      exists = await IOUtils.exists(path);
    } catch {
      throw new StorageFailure();
    }
    if (exists) {
      let cached: string;
      try {
        cached = await IOUtils.readUTF8(path);
      } catch {
        throw new StorageFailure();
      }
      if (
        await digest(cached) !== fixture.digest || cached !== fixture.source
      ) {
        throw new Error("content-addressed fixture cache is corrupt");
      }
    } else {
      try {
        await IOUtils.writeUTF8(path, fixture.source, {
          tmpPath: `${path}.tmp`,
          flush: true,
        });
      } catch {
        throw new StorageFailure();
      }
    }
    return { ...fixture };
  }

  async load(
    digestValue: string,
    version: FixtureVersion,
  ): Promise<M0FixtureValue> {
    const fixture = M0_FIXTURES[version];
    if (digestValue !== fixture.digest) {
      throw new Error("untrusted fixture digest");
    }
    const path = PathUtils.join(this.#directory, `${digestValue}.js`);
    let source: string;
    try {
      source = await IOUtils.readUTF8(path);
    } catch {
      throw new StorageFailure();
    }
    if (await digest(source) !== digestValue || source !== fixture.source) {
      throw new Error("cached fixture verification failed");
    }
    return { ...fixture, source };
  }
}

interface M0FixtureValue {
  version: FixtureVersion;
  digest: string;
  source: string;
}

export class M0FeatureHost {
  readonly #store: PackageStore;
  readonly #recordPath: string;
  readonly #windows = new Map<string, Window>();
  #record = defaultRecord();
  #running: M0ModuleHost | null = null;
  #epoch = Services.uuid.generateUUID().toString();
  #generation = 0;
  #activeToken: string | null = null;
  #queue: Promise<void> = Promise.resolve();
  #queued = 0;
  #closed = false;
  #volatileBlocked = false;
  #shutdownPromise: Promise<DisposeReport> | null = null;
  readonly #markerKeys = new Set<string>();

  private constructor(readonly root: string) {
    this.#store = new PackageStore(root);
    this.#recordPath = PathUtils.join(root, "state.json");
  }

  static async open(
    root: string,
    experimentalOptIn: boolean,
    testRoot = false,
  ): Promise<M0FeatureHost> {
    const mode = Services.prefs.getStringPref("nora.startup.mode", "");
    if (
      (mode !== "dev" && mode !== "test") ||
      !experimentalOptIn ||
      (testRoot
        ? PathUtils.parent(root) !== TEST_ROOT ||
          !/^m0-[0-9a-f-]+$/.test(PathUtils.filename(root))
        : root !== ROOT)
    ) throw new Error("M0 host is dev/test opt-in only");
    const opening = openingHosts.get(root);
    if (opening) return opening;
    const existing = owners.get(root);
    if (existing) return existing;
    const host = new M0FeatureHost(root);
    owners.set(root, host);
    const openingPromise = (async () => {
      try {
        if (testRoot && await IOUtils.exists(root)) {
          throw new Error("test root must be newly created");
        }
        await IOUtils.makeDirectory(root, {
          createAncestors: true,
          ignoreExisting: true,
          permissions: 0o700,
        });
        if (await IOUtils.exists(host.#recordPath)) {
          host.#record = validateRecord(
            await IOUtils.readJSON(host.#recordPath),
          );
          if (
            host.#record.desiredEnabled || host.#record.pending ||
            host.#record.lastOutcome?.status === "pending" ||
            host.#record.lastOutcome?.status === "storage_failed"
          ) host.#volatileBlocked = true;
        }
        return host;
      } catch (error) {
        owners.delete(root);
        throw error;
      }
    })();
    openingHosts.set(root, openingPromise);
    try {
      return await openingPromise;
    } finally {
      if (openingHosts.get(root) === openingPromise) openingHosts.delete(root);
    }
  }

  get diagnostics(): Readonly<M0Record> {
    return JSON.parse(JSON.stringify(this.#record)) as M0Record;
  }

  get runningLifecycleForTests(): M0Lifecycle | null {
    return this.#running?.lifecycleForTests ?? null;
  }

  get runningScopeDiagnosticsForTests(): {
    resources: number;
    childScopes: number;
  } {
    return this.#running?.scopeDiagnosticsForTests ??
      { resources: 0, childScopes: 0 };
  }

  get windowIdsForTests(): string[] {
    return [...this.#windows.keys()];
  }

  install(txId: string, expectedRevision: number, version: FixtureVersion) {
    return this.#execute(txId, expectedRevision, "install", version);
  }

  enable(txId: string, expectedRevision: number) {
    return this.#execute(txId, expectedRevision, "enable", null);
  }

  disable(txId: string, expectedRevision: number) {
    return this.#execute(txId, expectedRevision, "disable", null);
  }

  update(txId: string, expectedRevision: number, version: FixtureVersion) {
    return this.#execute(txId, expectedRevision, "update", version);
  }

  rollback(txId: string, expectedRevision: number) {
    return this.#execute(txId, expectedRevision, "rollback", null);
  }

  queryOutcome(txId: string): Outcome | "outcome_unknown_or_expired" {
    const found = this.#findOutcome(txId);
    return found === "outcome_unknown_or_expired"
      ? found
      : detachedOutcome(found);
  }

  #findOutcome(txId: string): Outcome | "outcome_unknown_or_expired" {
    if (this.#record.pending?.txId === txId) {
      return {
        txId,
        request: this.#record.pending.request,
        status: "pending",
        selectedVersion: this.#record.selectedVersion,
      };
    }
    if (this.#record.lastOutcome?.txId === txId) {
      return this.#record.lastOutcome;
    }
    return this.#record.recentOutcomes.find((entry) => entry.txId === txId) ??
      "outcome_unknown_or_expired";
  }

  retryExisting(txId: string): Outcome | "outcome_unknown_or_expired" {
    return this.queryOutcome(txId);
  }

  shutdown(): Promise<DisposeReport> {
    if (this.#shutdownPromise) return this.#shutdownPromise;
    this.#closed = true;
    this.#shutdownPromise = (async () => {
      await this.#queue;
      this.#activeToken = null;
      const result = this.#running
        ? await this.#running.stop("host-shutdown")
        : { errors: [], timedOut: false };
      this.#running = null;
      this.#windows.clear();
      this.#markerKeys.clear();
      if (
        result.errors.length === 0 && !result.timedOut &&
        !this.#record.blockedReason && !this.#volatileBlocked
      ) owners.delete(this.root);
      else this.#volatileBlocked = true;
      return result;
    })();
    return this.#shutdownPromise;
  }

  #enqueue<T>(operation: () => Promise<T>): Promise<T> {
    if (this.#closed) return Promise.reject(new Error("M0 host closed"));
    if (this.#queued >= 16) return Promise.reject(new Error("busy"));
    this.#queued += 1;
    const result = this.#queue.then(operation, operation);
    this.#queue = result.then(() => undefined, () => undefined).finally(() => {
      this.#queued -= 1;
    });
    return result;
  }

  async #write(
    next: Omit<M0Record, "stateRevision"> & { stateRevision?: number },
  ): Promise<M0Record> {
    const committed = {
      ...next,
      stateRevision: this.#record.stateRevision + 1,
    } as M0Record;
    try {
      await IOUtils.writeJSON(this.#recordPath, committed, {
        tmpPath: `${this.#recordPath}.tmp`,
        flush: true,
      });
    } catch {
      throw new StorageFailure();
    }
    this.#record = committed;
    return committed;
  }

  #execute(
    txId: string,
    expectedRevision: number,
    op: Operation,
    target: FixtureVersion | null,
  ): Promise<Outcome> {
    const request: Descriptor = {
      featureId: FEATURE_ID,
      op,
      expectedRevision,
      target,
      normalizedInput: `${op}:${target ?? "none"}`,
    };
    const known = this.#findOutcome(txId);
    if (known !== "outcome_unknown_or_expired") {
      if (JSON.stringify(known.request) !== JSON.stringify(request)) {
        return Promise.reject(new Error("transaction ID payload conflict"));
      }
      return Promise.resolve(detachedOutcome(known));
    }
    if (expectedRevision !== this.#record.stateRevision) {
      return Promise.reject(new Error("stale_revision"));
    }
    return this.#enqueue(async () => {
      if (this.#closed) throw new Error("M0 host closed");
      const queuedKnown = this.#findOutcome(txId);
      if (queuedKnown !== "outcome_unknown_or_expired") {
        if (JSON.stringify(queuedKnown.request) !== JSON.stringify(request)) {
          throw new Error("transaction ID payload conflict");
        }
        return detachedOutcome(queuedKnown);
      }
      if (expectedRevision !== this.#record.stateRevision) {
        throw new Error("stale_revision");
      }
      try {
        return detachedOutcome(await this.#perform(txId, request));
      } catch (error) {
        if (!(error instanceof StorageFailure)) throw error;
        if (this.#record.pending?.txId === txId) this.#volatileBlocked = true;
        return {
          txId,
          request,
          status: "storage_failed",
          selectedVersion: this.#record.selectedVersion,
        };
      }
    });
  }

  async #perform(txId: string, request: Descriptor): Promise<Outcome> {
    const before = this.#record;
    if (this.#volatileBlocked || this.#record.blockedReason) {
      throw new Error("quarantined");
    }
    if (request.op === "install") {
      if (before.selectedVersion !== null || !request.target) {
        throw new Error("invalid install");
      }
      const fixture = await this.#store.ensure(request.target);
      return await this.#terminal(txId, request, "succeeded", {
        ...before,
        selectedVersion: fixture.version,
        selectedDigest: fixture.digest,
        desiredEnabled: false,
        selectedHealth: "unverified",
        selectedHealthBuild: null,
      });
    }
    if (!before.selectedVersion || !before.selectedDigest) {
      throw new Error("not installed");
    }
    if (request.op === "disable") {
      if (!before.desiredEnabled) throw new Error("already disabled");
      const pending = { txId, request, phase: "PREPARED" };
      await this.#write({ ...before, desiredEnabled: false, pending });
      const stopped = this.#running
        ? await this.#running.stop("disable")
        : { errors: [], timedOut: false };
      this.#running = null;
      this.#activeToken = null;
      this.#windows.clear();
      this.#markerKeys.clear();
      if (stopped.errors.length || stopped.timedOut) {
        return await this.#quarantine(txId, request, "disable-cleanup-failure");
      }
      return await this.#terminal(txId, request, "succeeded", {
        ...this.#record,
        desiredEnabled: false,
        pending: null,
      });
    }
    if (request.op === "enable" && before.desiredEnabled) {
      throw new Error("already enabled");
    }
    const targetVersion = request.op === "enable"
      ? before.selectedVersion
      : request.op === "rollback"
      ? before.rollbackVersion
      : request.target;
    if (!targetVersion) throw new Error("missing target version");
    if (request.op === "rollback" && !this.#rollbackEligible(before)) {
      throw new Error("rollback target lacks current-build health proof");
    }
    const fixture = await this.#store.ensure(targetVersion);
    if (request.op === "enable" || before.desiredEnabled) {
      return await this.#activate(txId, request, before, fixture);
    }
    if (request.op === "update" || request.op === "rollback") {
      const previousIsHealthy = this.#selectedEligible(before);
      const keepOldRollback = request.op === "update" &&
        this.#rollbackEligible(before);
      const nextRollbackVersion = request.op === "update" && keepOldRollback
        ? before.rollbackVersion
        : previousIsHealthy
        ? before.selectedVersion
        : null;
      const nextRollbackDigest = request.op === "update" && keepOldRollback
        ? before.rollbackDigest
        : previousIsHealthy
        ? before.selectedDigest
        : null;
      const nextRollbackHealthBuild = request.op === "update" && keepOldRollback
        ? before.rollbackHealthBuild
        : previousIsHealthy
        ? before.selectedHealthBuild
        : null;
      return await this.#terminal(txId, request, "succeeded", {
        ...before,
        selectedVersion: fixture.version,
        selectedDigest: fixture.digest,
        selectedHealth: request.op === "rollback" ? "verified" : "unverified",
        selectedHealthBuild: request.op === "rollback"
          ? before.rollbackHealthBuild
          : null,
        rollbackVersion: nextRollbackVersion,
        rollbackDigest: nextRollbackDigest,
        rollbackHealthBuild: nextRollbackHealthBuild,
      });
    }
    throw new Error("unsupported operation");
  }

  #selectedEligible(record: M0Record): boolean {
    return record.selectedVersion !== null &&
      record.selectedDigest === M0_FIXTURES[record.selectedVersion].digest &&
      record.selectedHealth === "verified" &&
      record.selectedHealthBuild?.hostBuildId === HOST_BUILD_ID &&
      record.selectedHealthBuild.runtimeBuildId === Services.appinfo.appBuildID;
  }

  #rollbackEligible(record: M0Record): boolean {
    return record.rollbackVersion !== null &&
      record.rollbackDigest === M0_FIXTURES[record.rollbackVersion].digest &&
      record.rollbackHealthBuild?.hostBuildId === HOST_BUILD_ID &&
      record.rollbackHealthBuild.runtimeBuildId === Services.appinfo.appBuildID;
  }

  async #activate(
    txId: string,
    request: Descriptor,
    before: M0Record,
    fixture: M0FixtureValue,
  ): Promise<Outcome> {
    const verifiedFixture = await this.#store.load(
      fixture.digest,
      fixture.version,
    );
    const windows = this.#captureWindows();
    await this.#write({
      ...before,
      pending: { txId, request, phase: "PREPARED" },
    });
    const old = this.#running;
    if (old) {
      this.#activeToken = null;
      const stopped = await old.stop("replacement");
      if (stopped.errors.length || stopped.timedOut) {
        this.#running = null;
        return await this.#quarantine(txId, request, "old-cleanup-failure");
      }
      this.#running = null;
    }
    this.#windows.clear();
    for (const entry of windows) this.#windows.set(entry.id, entry.window);
    const generation = ++this.#generation;
    const token = `${this.#epoch}:${generation}`;
    const runtime = new M0ModuleHost(
      this.#epoch,
      generation,
      verifiedFixture.digest,
      verifiedFixture.source,
      (epoch, gen, json) => this.#broker(epoch, gen, json, token),
    );
    this.#activeToken = token;
    this.#running = runtime;
    try {
      await runtime.start(windows.map((entry) => entry.id));
      const build = {
        hostBuildId: HOST_BUILD_ID,
        runtimeBuildId: Services.appinfo.appBuildID,
      };
      this.#running = runtime;
      const previousWasHealthy = this.#selectedEligible(before);
      const preserveFallback =
        (request.op === "update" || request.op === "enable") &&
        this.#rollbackEligible(before);
      const nextRollbackVersion = previousWasHealthy
        ? before.selectedVersion
        : preserveFallback
        ? before.rollbackVersion
        : null;
      const nextRollbackDigest = previousWasHealthy
        ? before.selectedDigest
        : preserveFallback
        ? before.rollbackDigest
        : null;
      const nextRollbackHealthBuild = previousWasHealthy
        ? before.selectedHealthBuild
        : preserveFallback
        ? before.rollbackHealthBuild
        : null;
      let committed: M0Record;
      try {
        committed = await this.#write({
          ...this.#archiveTerminalBeforeReplacement(before, txId),
          selectedVersion: fixture.version,
          selectedDigest: fixture.digest,
          desiredEnabled: true,
          selectedHealth: "verified",
          selectedHealthBuild: build,
          rollbackVersion: nextRollbackVersion,
          rollbackDigest: nextRollbackDigest,
          rollbackHealthBuild: nextRollbackHealthBuild,
          pending: null,
          lastOutcome: {
            txId,
            request,
            status: "pending",
            selectedVersion: fixture.version,
          },
        });
      } catch {
        return await this.#finalizationStorageFailure(
          txId,
          request,
          runtime,
          "commit-storage-failure",
        );
      }
      try {
        return await this.#terminal(txId, request, "succeeded", committed);
      } catch {
        return await this.#finalizationStorageFailure(
          txId,
          request,
          runtime,
          "finalization-storage-failure",
        );
      }
    } catch (error) {
      this.#activeToken = null;
      const stopped = await runtime.stop("candidate-failed");
      this.#running = null;
      if (stopped.errors.length || stopped.timedOut) {
        this.#running = null;
        return await this.#quarantine(
          txId,
          request,
          "candidate-cleanup-failure",
        );
      }
      if (
        before.desiredEnabled && before.selectedVersion && before.selectedDigest
      ) {
        let previous: M0FixtureValue;
        try {
          previous = await this.#store.load(
            before.selectedDigest,
            before.selectedVersion,
          );
        } catch {
          return await this.#quarantine(
            txId,
            request,
            "restore-package-unavailable",
          );
        }
        const restore = new M0ModuleHost(
          this.#epoch,
          ++this.#generation,
          previous.digest,
          previous.source,
          (epoch, gen, json) =>
            this.#broker(epoch, gen, json, `${epoch}:${gen}`),
        );
        this.#activeToken = `${this.#epoch}:${this.#generation}`;
        this.#running = restore;
        try {
          await restore.start(windows.map((entry) => entry.id));
        } catch {
          this.#activeToken = null;
          await restore.stop("restore-failed");
          this.#running = null;
          return await this.#quarantine(txId, request, "restore-failure");
        }
        this.#running = restore;
        try {
          return await this.#terminal(txId, request, "failed_restored", {
            ...before,
            pending: null,
          });
        } catch {
          return await this.#finalizationStorageFailure(
            txId,
            request,
            restore,
            "restore-finalization-storage-failure",
          );
        }
      }
      this.#windows.clear();
      return await this.#terminal(txId, request, "failed_restored", {
        ...before,
        desiredEnabled: false,
        pending: null,
      }, String(error));
    }
  }

  #archiveTerminalBeforeReplacement(record: M0Record, txId: string): M0Record {
    const previous = record.lastOutcome;
    if (!previous || previous.txId === txId || previous.status === "pending") {
      return record;
    }
    return {
      ...record,
      recentOutcomes: [...record.recentOutcomes, previous].slice(-32),
    };
  }

  async #finalizationStorageFailure(
    txId: string,
    request: Descriptor,
    runtime: M0ModuleHost,
    reason: string,
  ): Promise<Outcome> {
    this.#activeToken = null;

    await runtime.stop(reason);
    this.#running = null;
    this.#windows.clear();
    this.#markerKeys.clear();
    this.#volatileBlocked = true;
    let selectedVersion = this.#record.selectedVersion;
    try {
      const durable = validateRecord(await IOUtils.readJSON(this.#recordPath));
      this.#record = durable;
      selectedVersion = durable.selectedVersion;
      const terminal = durable.lastOutcome;
      const unfinished = hasUnfinishedM0FinalizationForTests(
        durable.pending?.txId ?? null,
        terminal?.txId ?? null,
        terminal?.status ?? null,
        txId,
      );
      if (unfinished) {
        await this.#terminal(txId, request, "failed_quarantined", {
          ...durable,
          desiredEnabled: false,
          blockedReason: "finalization_storage_failure",
        });
      }
    } catch {
      return { txId, request, status: "storage_failed", selectedVersion };
    }
    return { txId, request, status: "storage_failed", selectedVersion };
  }

  #captureWindows(): WindowEntry[] {
    const entries: WindowEntry[] = [];
    const enumerator = Services.wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      const window = enumerator.getNext() as unknown as Window;
      if (!isEligibleM0Window(window)) continue;
      const id = Services.uuid.generateUUID().toString();
      entries.push({ id, window });
    }
    return entries;
  }

  #broker(
    epoch: string,
    generation: number,
    json: string,
    token: string,
  ): string {
    const runtime = this.#running;
    if (
      this.#activeToken !== token || epoch !== this.#epoch ||
      generation !== this.#generation || !runtime ||
      runtime.epoch !== epoch || runtime.generation !== generation ||
      runtime.scope.revoked
    ) {
      throw new Error("stale generation");
    }
    const value: unknown = JSON.parse(json);
    if (typeof value !== "object" || value === null) {
      throw new Error("invalid broker request");
    }
    const request = value as Record<string, unknown>;
    if (request.type !== "marker" || typeof request.windowId !== "string") {
      throw new Error("unsupported broker request");
    }
    const window = this.#windows.get(request.windowId);
    if (
      !window || !isEligibleM0Window(window)
    ) {
      throw new Error("ineligible window");
    }
    const scope = runtime.scope.child(request.windowId);
    if (scope.revoked) throw new Error("window scope revoked");
    const markerKey = `${token}:${request.windowId}`;
    if (this.#markerKeys.has(markerKey)) return JSON.stringify({ ok: true });
    const root = window.document.documentElement;
    const attribute = "data-floorp-hotfix-m0";
    const previous = root.getAttribute(attribute);
    this.#markerKeys.add(markerKey);
    const cleanup = () => {
      try {
        if (root.getAttribute(attribute) === token) {
          if (previous === null) root.removeAttribute(attribute);
          else root.setAttribute(attribute, previous);
        }
      } finally {
        this.#markerKeys.delete(markerKey);
      }
    };
    try {
      root.setAttribute(attribute, token);
      scope.register(cleanup);
    } catch (error) {
      this.#markerKeys.delete(markerKey);
      if (root.getAttribute(attribute) === token) {
        if (previous === null) root.removeAttribute(attribute);
        else root.setAttribute(attribute, previous);
      }
      throw error;
    }
    return JSON.stringify({ ok: true });
  }

  async #terminal(
    txId: string,
    request: Descriptor,
    status: OutcomeStatus,
    base: M0Record,
    _detail?: string,
  ): Promise<Outcome> {
    const entry: Outcome = {
      txId,
      request,
      status,
      selectedVersion: base.selectedVersion,
    };
    const recent = base.lastOutcome && base.lastOutcome.txId !== txId &&
        base.lastOutcome.status !== "pending"
      ? [...base.recentOutcomes, base.lastOutcome].slice(-32)
      : base.recentOutcomes;
    const next = await this.#write({
      ...base,
      pending: null,
      recentOutcomes: recent,
      lastOutcome: entry,
    });
    return detachedOutcome(next.lastOutcome as Outcome);
  }

  async #quarantine(
    txId: string,
    request: Descriptor,
    reason: string,
  ): Promise<Outcome> {
    this.#activeToken = null;
    this.#volatileBlocked = true;
    this.#windows.clear();
    this.#markerKeys.clear();
    const entry: Outcome = {
      txId,
      request,
      status: "failed_quarantined",
      selectedVersion: this.#record.selectedVersion,
    };
    try {
      const base = this.#record;
      return await this.#terminal(txId, request, "failed_quarantined", {
        ...base,
        desiredEnabled: false,
        blockedReason: reason,
      });
    } catch {
      return { ...entry, status: "storage_failed" };
    }
  }
}

export interface M0TestHostHandle {
  readonly root: string;
  readonly host: M0FeatureHost;
  cleanup(): Promise<DisposeReport>;
}

export async function getM0FeatureHostExperimental(
  experimentalOptIn: boolean,
): Promise<M0FeatureHost> {
  const mode = Services.prefs.getStringPref("nora.startup.mode", "");
  if (!experimentalOptIn || (mode !== "dev" && mode !== "test")) {
    throw new Error("M0 host is dev/test opt-in only");
  }
  return await M0FeatureHost.open(ROOT, experimentalOptIn);
}

export async function createM0FeatureHostForTests(
  experimentalOptIn: boolean,
  rootOverride?: string,
): Promise<M0TestHostHandle> {
  const suffix = Services.uuid.generateUUID().toString().replace(/[{}]/g, "")
    .toLowerCase();
  const root = rootOverride ?? PathUtils.join(TEST_ROOT, `m0-${suffix}`);
  const host = await M0FeatureHost.open(root, experimentalOptIn, true);
  let cleanupPromise: Promise<DisposeReport> | null = null;
  return {
    root,
    host,
    cleanup: () => {
      if (cleanupPromise) return cleanupPromise;
      cleanupPromise = (async () => {
        const report = await host.shutdown();
        if (
          report.errors.length === 0 && !report.timedOut && !owners.has(root)
        ) {
          await IOUtils.remove(root, { recursive: true, ignoreAbsent: true });
        }
        return report;
      })();
      return cleanupPromise;
    },
  };
}
