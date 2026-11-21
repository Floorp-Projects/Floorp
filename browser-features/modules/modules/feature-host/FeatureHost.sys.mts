// SPDX-License-Identifier: MPL-2.0

import { type FixtureVersion, M0_FIXTURES } from "./M0Fixtures.sys.mts";
import {
  type DisposeReport,
  type M0Lifecycle,
  M0ModuleHost,
} from "./M0ModuleHost.sys.mts";

const FEATURE_ID = "experimental-local-sample";
const HOST_BUILD_ID = "m0-schema-2";
const ROOT = PathUtils.join(PathUtils.profileDir, "floorp-hotfix-m0");
const TEST_ROOT = PathUtils.join(
  PathUtils.profileDir,
  "floorp-hotfix-m0-tests",
);
const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
);

type Operation = "install" | "enable" | "disable" | "update" | "rollback";
type RequestOperation = Operation | "startup_recovery";
type PendingPhase = "PREPARED" | "OLD_STOPPED" | "CANDIDATE_READY";
type OutcomeStatus =
  | "pending"
  | "committed_pending_publish"
  | "succeeded"
  | "failed_restored"
  | "failed_quarantined"
  | "storage_failed";

interface Descriptor {
  featureId: string;
  op: RequestOperation;
  expectedRevision: number | null;
  target: FixtureVersion | null;
  recoveryEpoch: string | null;
  normalizedInput: string;
}

interface Outcome {
  txId: string;
  request: Descriptor;
  status: OutcomeStatus;
  selectedVersion: FixtureVersion | null;
}

interface M0Record {
  schemaVersion: 2;
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
  pending: PendingTransaction | null;
  attempt: AttemptEpisode | null;
  blockedReason: string | null;
  recentOutcomes: Outcome[];
  lastOutcome: Outcome | null;
}

interface RecordSnapshot {
  selectedVersion: FixtureVersion | null;
  selectedDigest: string | null;
  selectedHealth: "verified" | "unverified";
  selectedHealthBuild: M0Record["selectedHealthBuild"];
  rollbackVersion: FixtureVersion | null;
  rollbackDigest: string | null;
  rollbackHealthBuild: M0Record["rollbackHealthBuild"];
  desiredEnabled: boolean;
}

interface PendingTransaction {
  op: RequestOperation;
  txId: string;
  request: Descriptor;
  before: RecordSnapshot;
  targetVersion: FixtureVersion | null;
  targetDigest: string | null;
  phase: PendingPhase;
}

interface AttemptEpisode {
  recoveryEpoch: string;
  originTxId: string;
  request: Descriptor;
  targetDigest: string;
  attemptedDigests: string[];
  rejectedDigests: string[];
}

interface WindowEntry {
  id: string;
  window: Window;
}

const owners = new Map<string, M0FeatureHost>();
const openingHosts = new Map<string, Promise<M0FeatureHost>>();
let m0PayloadEvaluations = 0;

export function m0PayloadEvaluationsForTests(): number {
  return m0PayloadEvaluations;
}

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
  if (terminalTxId === txId) {
    return terminalStatus === "pending" ||
      terminalStatus === "committed_pending_publish";
  }
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
  constructor(readonly uncertain = true) {
    super("storage_failed");
    this.name = "StorageFailure";
  }
}

function defaultRecord(): M0Record {
  return {
    schemaVersion: 2,
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
    attempt: null,
    blockedReason: null,
    recentOutcomes: [],
    lastOutcome: null,
  };
}

function snapshotRecord(record: M0Record): RecordSnapshot {
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

function withSnapshot(
  record: M0Record,
  snapshot: RecordSnapshot,
): M0Record {
  return { ...record, ...snapshot };
}

function isVersion(value: unknown): value is FixtureVersion {
  return value === "v1" || value === "v2" || value === "broken";
}

function isTxId(value: unknown): value is string {
  return typeof value === "string" && value.length > 0 && value.length <= 128 &&
    /^[A-Za-z0-9._:-]+$/.test(value);
}

function isKnownDigest(value: unknown): value is string {
  return typeof value === "string" &&
    Object.values(M0_FIXTURES).some((fixture) => fixture.digest === value);
}

function isOperation(value: unknown): value is RequestOperation {
  return value === "install" || value === "enable" || value === "disable" ||
    value === "update" || value === "rollback" ||
    value === "startup_recovery";
}

function hasExactKeys(value: object, keys: readonly string[]): boolean {
  const actual = Object.keys(value);
  return actual.length === keys.length &&
    actual.every((key) => keys.includes(key));
}

function isDescriptor(value: unknown): value is Descriptor {
  if (
    typeof value !== "object" || value === null ||
    !hasExactKeys(value, [
      "featureId",
      "op",
      "expectedRevision",
      "target",
      "recoveryEpoch",
      "normalizedInput",
    ])
  ) return false;
  const descriptor = value as Partial<Descriptor>;
  if (
    descriptor.featureId !== FEATURE_ID || !isOperation(descriptor.op) ||
    !(descriptor.expectedRevision === null ||
      (typeof descriptor.expectedRevision === "number" &&
        Number.isSafeInteger(descriptor.expectedRevision) &&
        descriptor.expectedRevision >= 0)) ||
    !(descriptor.target === null || isVersion(descriptor.target)) ||
    !(descriptor.recoveryEpoch === null || isTxId(descriptor.recoveryEpoch)) ||
    typeof descriptor.normalizedInput !== "string" ||
    descriptor.normalizedInput.length > 256
  ) return false;
  if (descriptor.op === "startup_recovery") {
    return descriptor.expectedRevision === null && descriptor.target === null &&
      descriptor.recoveryEpoch !== null &&
      descriptor.normalizedInput ===
        `startup_recovery:${descriptor.recoveryEpoch}`;
  }
  return descriptor.expectedRevision !== null &&
    descriptor.recoveryEpoch === null &&
    (descriptor.op === "install" || descriptor.op === "update"
      ? descriptor.target !== null
      : descriptor.target === null) &&
    descriptor.normalizedInput ===
      `${descriptor.op}:${descriptor.target ?? "none"}`;
}

function isOutcome(value: unknown): value is Outcome {
  if (
    typeof value !== "object" || value === null ||
    !hasExactKeys(value, ["txId", "request", "status", "selectedVersion"])
  ) return false;
  const outcome = value as Partial<Outcome>;
  return isTxId(outcome.txId) && isDescriptor(outcome.request) &&
    (outcome.status === "pending" ||
      outcome.status === "committed_pending_publish" ||
      outcome.status === "succeeded" ||
      outcome.status === "failed_restored" ||
      outcome.status === "failed_quarantined" ||
      outcome.status === "storage_failed") &&
    (outcome.selectedVersion === null || isVersion(outcome.selectedVersion));
}

function isHealthBuild(
  value: unknown,
): value is M0Record["selectedHealthBuild"] {
  if (value === null) return true;
  if (
    typeof value !== "object" || value === null ||
    !hasExactKeys(value, ["hostBuildId", "runtimeBuildId"])
  ) return false;
  const build = value as { hostBuildId?: unknown; runtimeBuildId?: unknown };
  return typeof build.hostBuildId === "string" &&
    build.hostBuildId.length > 0 && build.hostBuildId.length <= 128 &&
    typeof build.runtimeBuildId === "string" &&
    build.runtimeBuildId.length > 0 && build.runtimeBuildId.length <= 128;
}

function isSnapshot(value: unknown): value is RecordSnapshot {
  if (
    typeof value !== "object" || value === null ||
    !hasExactKeys(value, [
      "selectedVersion",
      "selectedDigest",
      "selectedHealth",
      "selectedHealthBuild",
      "rollbackVersion",
      "rollbackDigest",
      "rollbackHealthBuild",
      "desiredEnabled",
    ])
  ) return false;
  const snapshot = value as Partial<RecordSnapshot>;
  return typeof snapshot.desiredEnabled === "boolean" &&
    (snapshot.selectedVersion === null ||
      isVersion(snapshot.selectedVersion)) &&
    (snapshot.rollbackVersion === null ||
      isVersion(snapshot.rollbackVersion)) &&
    (snapshot.selectedDigest === null ||
      isKnownDigest(snapshot.selectedDigest)) &&
    (snapshot.rollbackDigest === null ||
      isKnownDigest(snapshot.rollbackDigest)) &&
    (snapshot.selectedHealth === "verified" ||
      snapshot.selectedHealth === "unverified") &&
    isHealthBuild(snapshot.selectedHealthBuild) &&
    isHealthBuild(snapshot.rollbackHealthBuild) &&
    ((snapshot.selectedVersion === null) ===
      (snapshot.selectedDigest === null)) &&
    ((snapshot.rollbackVersion === null) ===
      (snapshot.rollbackDigest === null)) &&
    (snapshot.selectedVersion !== null ||
      (snapshot.selectedHealth === "unverified" &&
        snapshot.selectedHealthBuild === null)) &&
    ((snapshot.selectedHealth === "verified") ===
      (snapshot.selectedHealthBuild !== null)) &&
    ((snapshot.rollbackVersion === null) ===
      (snapshot.rollbackHealthBuild === null)) &&
    (snapshot.selectedVersion === null ||
      snapshot.selectedDigest ===
        M0_FIXTURES[snapshot.selectedVersion].digest) &&
    (snapshot.rollbackVersion === null ||
      snapshot.rollbackDigest === M0_FIXTURES[snapshot.rollbackVersion].digest);
}

function isPending(value: unknown): value is PendingTransaction {
  if (
    typeof value !== "object" || value === null ||
    !hasExactKeys(value, [
      "op",
      "txId",
      "request",
      "before",
      "targetVersion",
      "targetDigest",
      "phase",
    ])
  ) return false;
  const pending = value as Partial<PendingTransaction>;
  return isOperation(pending.op) && isTxId(pending.txId) &&
    isDescriptor(pending.request) && pending.request.op === pending.op &&
    isSnapshot(pending.before) &&
    (pending.targetVersion === null || isVersion(pending.targetVersion)) &&
    (pending.targetDigest === null || isKnownDigest(pending.targetDigest)) &&
    pending.targetVersion !== null && pending.targetDigest !== null &&
    (pending.targetVersion === null ||
      pending.targetDigest === M0_FIXTURES[pending.targetVersion].digest) &&
    (pending.phase === "PREPARED" || pending.phase === "OLD_STOPPED" ||
      pending.phase === "CANDIDATE_READY");
}

function isAttemptEpisode(value: unknown): value is AttemptEpisode {
  if (
    typeof value !== "object" || value === null ||
    !hasExactKeys(value, [
      "recoveryEpoch",
      "originTxId",
      "request",
      "targetDigest",
      "attemptedDigests",
      "rejectedDigests",
    ])
  ) return false;
  const episode = value as Partial<AttemptEpisode>;
  if (
    !isTxId(episode.recoveryEpoch) || !isTxId(episode.originTxId) ||
    !isDescriptor(episode.request) || !isKnownDigest(episode.targetDigest) ||
    !Array.isArray(episode.attemptedDigests) ||
    !Array.isArray(episode.rejectedDigests) ||
    episode.attemptedDigests.length > 2 || episode.rejectedDigests.length > 2 ||
    !episode.attemptedDigests.every(isKnownDigest) ||
    !episode.rejectedDigests.every(isKnownDigest)
  ) return false;
  const attempted = episode.attemptedDigests;
  const rejected = episode.rejectedDigests;
  return new Set(attempted).size === attempted.length &&
    new Set(rejected).size === rejected.length &&
    attempted.length + rejected.length <= 2 &&
    !attempted.some((digestValue) => rejected.includes(digestValue)) &&
    (episode.request.op !== "startup_recovery" ||
      episode.request.recoveryEpoch === episode.recoveryEpoch);
}

function validateRecord(value: unknown): M0Record {
  if (typeof value !== "object" || value === null) {
    throw new Error("invalid M0 state record");
  }
  const record = value as Partial<M0Record>;
  const keys = [
    "schemaVersion",
    "featureId",
    "desiredEnabled",
    "selectedVersion",
    "selectedDigest",
    "selectedHealth",
    "selectedHealthBuild",
    "rollbackVersion",
    "rollbackDigest",
    "rollbackHealthBuild",
    "stateRevision",
    "pending",
    "attempt",
    "blockedReason",
    "recentOutcomes",
    "lastOutcome",
  ];
  if (
    Object.keys(value).some((key) => !keys.includes(key)) ||
    record.schemaVersion !== 2 || record.featureId !== FEATURE_ID ||
    typeof record.desiredEnabled !== "boolean" ||
    !Number.isSafeInteger(record.stateRevision) ||
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
    record.recentOutcomes.some((outcome) =>
      outcome.status === "pending" ||
      outcome.status === "committed_pending_publish" ||
      outcome.status === "storage_failed"
    ) ||
    !(record.lastOutcome === null || isOutcome(record.lastOutcome)) ||
    (record.lastOutcome !== null &&
      (record.lastOutcome.status === "pending" ||
        record.lastOutcome.status === "storage_failed")) ||
    !(record.pending === null || isPending(record.pending)) ||
    !(record.attempt === null || isAttemptEpisode(record.attempt)) ||
    !(record.blockedReason === null ||
      typeof record.blockedReason === "string" &&
        record.blockedReason.length > 0 &&
        record.blockedReason.length <= 128) ||
    (record.selectedVersion === null) !== (record.selectedDigest === null) ||
    (record.selectedVersion === null &&
      (record.selectedHealth !== "unverified" ||
        record.selectedHealthBuild !== null)) ||
    (record.rollbackVersion === null) !== (record.rollbackDigest === null) ||
    (record.selectedVersion !== null &&
      record.selectedDigest !== M0_FIXTURES[record.selectedVersion].digest) ||
    (record.rollbackVersion !== null &&
      record.rollbackDigest !== M0_FIXTURES[record.rollbackVersion].digest) ||
    (record.desiredEnabled && record.selectedVersion === null) ||
    (record.pending !== null &&
      (record.pending.txId === "" ||
        record.pending.request.op === "startup_recovery" &&
          record.pending.request.recoveryEpoch !== record.pending.txId)) ||
    (record.pending !== null && record.attempt !== null &&
      (record.attempt.originTxId !== record.pending.txId ||
        record.attempt.targetDigest !== record.pending.targetDigest ||
        JSON.stringify(record.attempt.request) !==
          JSON.stringify(record.pending.request))) ||
    (record.pending === null && record.attempt !== null &&
      (record.lastOutcome?.txId !== record.attempt.originTxId ||
        JSON.stringify(record.lastOutcome.request) !==
          JSON.stringify(record.attempt.request) ||
        record.lastOutcome.status !== "committed_pending_publish" &&
          record.lastOutcome.status !== "failed_quarantined")) ||
    (record.lastOutcome?.status === "committed_pending_publish" &&
      (record.attempt === null ||
        record.attempt.originTxId !== record.lastOutcome.txId ||
        record.lastOutcome.selectedVersion === null ||
        !record.attempt.attemptedDigests.includes(
          M0_FIXTURES[record.lastOutcome.selectedVersion].digest,
        ))) ||
    (record.lastOutcome?.status === "failed_quarantined" &&
      record.blockedReason === null)
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

const HOST_CAPABILITIES = new Set(["window-marker"]);

export function isM0FixtureCompatibleForTests(
  version: FixtureVersion,
  hostBuildId: string,
  runtimeBuildId: string,
  availableCapabilities: readonly string[],
): boolean {
  const compatibility = M0_FIXTURES[version].compatibility;
  return compatibility.hostSchema === 2 &&
    compatibility.hostBuildId === HOST_BUILD_ID &&
    hostBuildId === HOST_BUILD_ID &&
    compatibility.runtimeBuildIds.includes(runtimeBuildId) &&
    compatibility.capabilities.every((capability) =>
      HOST_CAPABILITIES.has(capability) &&
      availableCapabilities.includes(capability)
    );
}

class PackageStore {
  readonly #directory: string;

  constructor(root: string) {
    this.#directory = PathUtils.join(root, "packages");
  }

  async ensure(version: FixtureVersion): Promise<M0FixtureValue> {
    const fixture = M0_FIXTURES[version];
    if (
      !isM0FixtureCompatibleForTests(
        version,
        HOST_BUILD_ID,
        Services.appinfo.appBuildID,
        [...HOST_CAPABILITIES],
      )
    ) throw new Error("fixture is incompatible with this host/runtime");
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
    if (
      !isM0FixtureCompatibleForTests(
        version,
        HOST_BUILD_ID,
        Services.appinfo.appBuildID,
        [...HOST_CAPABILITIES],
      )
    ) throw new Error("fixture is incompatible with this host/runtime");
    if (digestValue !== fixture.digest) {
      throw new Error("untrusted fixture digest");
    }
    const path = PathUtils.join(this.#directory, `${digestValue}.js`);
    let exists: boolean;
    try {
      exists = await IOUtils.exists(path);
    } catch {
      throw new StorageFailure();
    }
    if (!exists) throw new Error("cached fixture is missing");
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
  #nextWriteFault: {
    mode: "before_replace" | "after_replace";
    skip: number;
  } | null = null;
  #evaluations = 0;
  readonly #markerKeys = new Set<string>();
  readonly #testWritesEnabled: boolean;

  private constructor(readonly root: string, testWritesEnabled: boolean) {
    this.#testWritesEnabled = testWritesEnabled;
    this.#store = new PackageStore(root);
    this.#recordPath = PathUtils.join(root, "state.json");
  }

  static async open(
    root: string,
    experimentalOptIn: boolean,
    testRoot = false,
    reopenTestRoot = false,
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
    const host = new M0FeatureHost(root, testRoot);
    owners.set(root, host);
    const openingPromise = (async () => {
      try {
        if (testRoot && !reopenTestRoot && await IOUtils.exists(root)) {
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
            !host.#record.blockedReason &&
            host.#record.lastOutcome?.status !== "failed_quarantined" &&
            host.#record.selectedHealth === "verified" &&
            !host.#selectedEligible(host.#record)
          ) {
            await host.#write({
              ...host.#record,
              selectedHealth: "unverified",
              selectedHealthBuild: null,
            });
          }
        }
        await host.#recover();
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

  async #recover(): Promise<void> {
    let record = this.#record;
    if (
      record.blockedReason ||
      record.lastOutcome?.status === "failed_quarantined"
    ) {
      this.#volatileBlocked = true;
      return;
    }
    if (!record.desiredEnabled) {
      if (!record.pending) return;
      const pending = record.pending;
      const disabledBefore = withSnapshot(record, pending.before);
      const status = pending.op === "disable" ? "succeeded" : "failed_restored";
      await this.#terminal(pending.txId, pending.request, status, {
        ...disabledBefore,
        desiredEnabled: false,
        pending: null,
        attempt: null,
      });
      return;
    }

    const committed = record.pending === null &&
      record.lastOutcome?.status === "committed_pending_publish";
    const pending = record.pending;
    if (
      pending && (pending.op === "install" || pending.op === "enable" ||
        !pending.before.desiredEnabled)
    ) {
      await this.#terminal(pending.txId, pending.request, "failed_restored", {
        ...withSnapshot(record, pending.before),
        desiredEnabled: false,
        pending: null,
        attempt: null,
      });
      return;
    }

    const txId = committed ? record.attempt!.originTxId : pending?.txId;
    const request = committed ? record.attempt!.request : pending?.request;
    if (committed || pending) {
      if (!txId || !request) throw new Error("invalid recovery origin");
      let base = pending ? withSnapshot(record, pending.before) : record;
      let targetVersion: FixtureVersion | null = null;
      const episode = record.attempt;
      const tried = new Set([
        ...(episode?.attemptedDigests ?? []),
        ...(episode?.rejectedDigests ?? []),
      ]);
      if (
        pending?.op === "startup_recovery" && pending.targetVersion &&
        pending.targetDigest && !tried.has(pending.targetDigest)
      ) {
        targetVersion = pending.targetVersion;
      } else if (
        pending && pending.op !== "startup_recovery" &&
        this.#selectedEligible(base) && base.selectedDigest &&
        !tried.has(base.selectedDigest)
      ) {
        targetVersion = base.selectedVersion;
      } else if (
        (!pending || pending.op === "startup_recovery") &&
        this.#rollbackEligible(record) && record.rollbackVersion &&
        record.rollbackDigest && !tried.has(record.rollbackDigest)
      ) {
        targetVersion = record.rollbackVersion;
        base = {
          ...record,
          rollbackVersion: null,
          rollbackDigest: null,
          rollbackHealthBuild: null,
        };
      }
      if (!targetVersion) {
        await this.#quarantine(txId, request, "recovery-attempt-exhausted");
        return;
      }
      const targetDigest = M0_FIXTURES[targetVersion].digest;
      const recoveryPending: PendingTransaction = pending ?? {
        op: request.op,
        txId,
        request,
        before: snapshotRecord(record),
        targetVersion,
        targetDigest,
        phase: "OLD_STOPPED",
      };
      const attempt = episode ?? this.#newAttempt(txId, request, targetDigest);
      await this.#write({
        ...record,
        ...(!committed ? {} : { lastOutcome: record.lastOutcome }),
        pending: {
          ...recoveryPending,
          targetVersion,
          targetDigest,
          phase: "OLD_STOPPED",
        },
        attempt: { ...attempt, targetDigest },
      });
      let fixture: M0FixtureValue;
      try {
        fixture = await this.#store.load(targetDigest, targetVersion);
      } catch (error) {
        if (error instanceof StorageFailure) throw error;
        await this.#rejectRecoveryCandidate(
          txId,
          request,
          this.#record.pending!,
          targetVersion,
          base,
        );
        return;
      }
      await this.#activate(
        txId,
        request,
        base,
        fixture,
        request.op === "startup_recovery" ? "succeeded" : "failed_restored",
      );
      return;
    }

    if (!record.selectedVersion || !record.selectedDigest) {
      this.#volatileBlocked = true;
      throw new Error("enabled record has no selected fixture");
    }
    const startupVersion = record.selectedVersion;
    const startupDigest = record.selectedDigest;
    const recoveryEpoch = Services.uuid.generateUUID().toString()
      .replace(/[{}]/g, "").toLowerCase();
    const startupRequest: Descriptor = {
      featureId: FEATURE_ID,
      op: "startup_recovery",
      expectedRevision: null,
      target: null,
      recoveryEpoch,
      normalizedInput: `startup_recovery:${recoveryEpoch}`,
    };
    const startupTxId = recoveryEpoch;
    const pendingRecovery: PendingTransaction = {
      op: "startup_recovery",
      txId: startupTxId,
      request: startupRequest,
      before: snapshotRecord(record),
      targetVersion: startupVersion,
      targetDigest: startupDigest,
      phase: "PREPARED",
    };
    const attempt = this.#newAttempt(
      startupTxId,
      startupRequest,
      startupDigest,
    );
    record = await this.#write({
      ...record,
      pending: pendingRecovery,
      attempt,
    });
    let fixture: M0FixtureValue;
    try {
      fixture = await this.#store.load(
        startupDigest,
        startupVersion,
      );
    } catch (error) {
      if (error instanceof StorageFailure) throw error;
      await this.#rejectRecoveryCandidate(
        startupTxId,
        startupRequest,
        this.#record.pending!,
        startupVersion,
        record,
      );
      return;
    }
    await this.#activate(
      startupTxId,
      startupRequest,
      record,
      fixture,
      "succeeded",
    );
  }

  #newAttempt(
    txId: string,
    request: Descriptor,
    targetDigest: string,
  ): AttemptEpisode {
    return {
      recoveryEpoch: request.recoveryEpoch ??
        Services.uuid.generateUUID().toString().replace(/[{}]/g, "")
          .toLowerCase(),
      originTxId: txId,
      request,
      targetDigest,
      attemptedDigests: [],
      rejectedDigests: [],
    };
  }

  async #rejectRecoveryCandidate(
    txId: string,
    request: Descriptor,
    pending: PendingTransaction,
    targetVersion: FixtureVersion,
    base: M0Record,
  ): Promise<void> {
    const targetDigest = M0_FIXTURES[targetVersion].digest;
    const episode = this.#record.attempt ??
      this.#newAttempt(txId, request, targetDigest);
    if (
      episode.attemptedDigests.includes(targetDigest) ||
      episode.rejectedDigests.includes(targetDigest) ||
      episode.attemptedDigests.length + episode.rejectedDigests.length >= 2
    ) {
      await this.#quarantine(txId, request, "recovery-attempt-exhausted");
      return;
    }
    let record = await this.#write({
      ...this.#record,
      pending,
      attempt: {
        ...episode,
        targetDigest,
        rejectedDigests: [...episode.rejectedDigests, targetDigest],
      },
    });
    const mayFallback = request.op === "startup_recovery" ||
      record.lastOutcome?.txId === txId &&
        record.lastOutcome.status === "committed_pending_publish";
    if (
      !mayFallback || !this.#rollbackEligible(record) ||
      !record.rollbackVersion || !record.rollbackDigest ||
      episode.attemptedDigests.includes(record.rollbackDigest) ||
      episode.rejectedDigests.includes(record.rollbackDigest) ||
      episode.attemptedDigests.length + episode.rejectedDigests.length >= 2
    ) {
      await this.#quarantine(txId, request, "recovery-preflight-rejected");
      return;
    }
    const fallbackVersion = record.rollbackVersion;
    const fallbackDigest = record.rollbackDigest;
    const fallbackPending = {
      ...pending,
      targetVersion: fallbackVersion,
      targetDigest: fallbackDigest,
      phase: "OLD_STOPPED" as const,
    };
    const nextAttempt = { ...record.attempt!, targetDigest: fallbackDigest };
    record = await this.#write({
      ...record,
      pending: fallbackPending,
      attempt: nextAttempt,
    });
    let fixture: M0FixtureValue;
    try {
      fixture = await this.#store.load(fallbackDigest, fallbackVersion);
    } catch (error) {
      if (error instanceof StorageFailure) throw error;
      const attempt = record.attempt!;
      if (
        attempt.attemptedDigests.length + attempt.rejectedDigests.length < 2
      ) {
        await this.#write({
          ...record,
          attempt: {
            ...attempt,
            rejectedDigests: [...attempt.rejectedDigests, fallbackDigest],
          },
        });
      }
      await this.#quarantine(txId, request, "recovery-fallback-rejected");
      return;
    }
    await this.#activate(
      txId,
      request,
      {
        ...base,
        rollbackVersion: null,
        rollbackDigest: null,
        rollbackHealthBuild: null,
      },
      fixture,
      request.op === "startup_recovery" ? "succeeded" : "failed_restored",
    );
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

  get evaluationCountForTests(): number {
    return this.#evaluations;
  }

  setNextWriteFaultForTests(
    mode: "before_replace" | "after_replace",
    skip = 0,
  ): void {
    if (!this.#testWritesEnabled) throw new Error("test write fault disabled");
    if (!Number.isInteger(skip) || skip < 0 || skip > 32) {
      throw new Error("invalid write fault offset");
    }
    this.#nextWriteFault = { mode, skip };
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
    const nextRevision = this.#record.stateRevision + 1;
    if (!Number.isSafeInteger(nextRevision)) {
      throw new Error("state revision exhausted");
    }
    const committed = {
      ...next,
      stateRevision: nextRevision,
    } as M0Record;
    validateRecord(committed);
    const plan = this.#nextWriteFault;
    const fault = plan?.skip === 0 ? plan.mode : null;
    if (plan) {
      if (plan.skip === 0) this.#nextWriteFault = null;
      else this.#nextWriteFault = { ...plan, skip: plan.skip - 1 };
    }
    if (fault === "before_replace") throw new StorageFailure(false);
    try {
      await IOUtils.writeJSON(this.#recordPath, committed, {
        tmpPath: `${this.#recordPath}.tmp`,
        flush: true,
      });
    } catch {
      throw new StorageFailure();
    }
    if (fault === "after_replace") throw new StorageFailure();
    this.#record = committed;
    return committed;
  }

  #execute(
    txId: string,
    expectedRevision: number,
    op: Operation,
    target: FixtureVersion | null,
  ): Promise<Outcome> {
    if (!isTxId(txId)) {
      return Promise.reject(new Error("invalid transaction ID"));
    }
    if (!Number.isSafeInteger(expectedRevision) || expectedRevision < 0) {
      return Promise.reject(new Error("invalid expected revision"));
    }
    const request: Descriptor = {
      featureId: FEATURE_ID,
      op,
      expectedRevision,
      target,
      recoveryEpoch: null,
      normalizedInput: `${op}:${target ?? "none"}`,
    };
    if (!isDescriptor(request)) {
      return Promise.reject(new Error("invalid transaction descriptor"));
    }
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
        if (error.uncertain || this.#record.pending?.txId === txId) {
          this.#volatileBlocked = true;
        }
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
    if (
      this.#record.pending ||
      this.#record.lastOutcome?.status === "committed_pending_publish"
    ) throw new Error("recovery required before new transaction");
    if (request.op === "install") {
      if (before.selectedVersion !== null || !request.target) {
        throw new Error("invalid install");
      }
      const fixture = await this.#store.ensure(request.target);
      await this.#write({
        ...before,
        pending: {
          op: request.op,
          txId,
          request,
          before: snapshotRecord(before),
          targetVersion: fixture.version,
          targetDigest: fixture.digest,
          phase: "PREPARED",
        },
      });
      return await this.#terminal(txId, request, "succeeded", {
        ...this.#record,
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
      const pending: PendingTransaction = {
        op: request.op,
        txId,
        request,
        before: snapshotRecord(before),
        targetVersion: before.selectedVersion,
        targetDigest: before.selectedDigest,
        phase: "PREPARED",
      };
      await this.#write({
        ...before,
        desiredEnabled: false,
        pending,
        attempt: null,
      });
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
        attempt: null,
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
      await this.#write({
        ...before,
        pending: {
          op: request.op,
          txId,
          request,
          before: snapshotRecord(before),
          targetVersion: fixture.version,
          targetDigest: fixture.digest,
          phase: "PREPARED",
        },
      });
      return await this.#terminal(txId, request, "succeeded", {
        ...this.#record,
        selectedVersion: fixture.version,
        selectedDigest: fixture.digest,
        selectedHealth: request.op === "rollback" ? "verified" : "unverified",
        selectedHealthBuild: request.op === "rollback"
          ? before.rollbackHealthBuild
          : null,
        rollbackVersion: nextRollbackVersion,
        rollbackDigest: nextRollbackDigest,
        rollbackHealthBuild: nextRollbackHealthBuild,
        pending: null,
        attempt: null,
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
    terminalStatus: "succeeded" | "failed_restored" = "succeeded",
  ): Promise<Outcome> {
    const verifiedFixture = fixture;
    const windows = this.#captureWindows();
    let pending = this.#record.pending?.txId === txId ? this.#record.pending : {
      op: request.op,
      txId,
      request,
      before: snapshotRecord(before),
      targetVersion: fixture.version,
      targetDigest: fixture.digest,
      phase: "PREPARED" as const,
    };
    if (this.#record.pending?.txId !== txId) {
      await this.#write({ ...before, pending, attempt: null });
    }
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
    pending = {
      ...pending,
      targetVersion: fixture.version,
      targetDigest: fixture.digest,
      phase: "OLD_STOPPED",
    };
    const episode = this.#record.attempt ??
      this.#newAttempt(txId, request, fixture.digest);
    await this.#write({
      ...this.#record,
      pending,
      attempt: { ...episode, targetDigest: fixture.digest },
    });
    if (
      episode.attemptedDigests.includes(fixture.digest) ||
      episode.rejectedDigests.includes(fixture.digest) ||
      episode.attemptedDigests.length + episode.rejectedDigests.length >= 2
    ) {
      return await this.#quarantine(
        txId,
        request,
        "recovery-attempt-exhausted",
      );
    }
    await this.#write({
      ...this.#record,
      pending,
      attempt: {
        ...episode,
        targetDigest: fixture.digest,
        attemptedDigests: [...episode.attemptedDigests, fixture.digest],
      },
    });
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
      this.#evaluations += 1;
      m0PayloadEvaluations += 1;
      await runtime.start(windows.map((entry) => entry.id));
      pending = { ...pending, phase: "CANDIDATE_READY" };
      await this.#write({ ...this.#record, pending });
      const build = {
        hostBuildId: HOST_BUILD_ID,
        runtimeBuildId: Services.appinfo.appBuildID,
      };
      this.#running = runtime;
      const previousWasHealthy = this.#selectedEligible(before);
      const startupFallback = request.op === "startup_recovery" &&
        before.selectedDigest !== fixture.digest;
      const preserveRollback = this.#rollbackEligible(before);
      const preserveBeforePair = terminalStatus === "failed_restored";
      const nextRollbackVersion = preserveBeforePair
        ? before.rollbackVersion
        : startupFallback
        ? null
        : request.op === "startup_recovery" || request.op === "enable"
        ? preserveRollback ? before.rollbackVersion : null
        : previousWasHealthy
        ? before.selectedVersion
        : preserveRollback
        ? before.rollbackVersion
        : null;
      const nextRollbackDigest = nextRollbackVersion === before.rollbackVersion
        ? before.rollbackDigest
        : nextRollbackVersion === before.selectedVersion
        ? before.selectedDigest
        : null;
      const nextRollbackHealthBuild =
        nextRollbackVersion === before.rollbackVersion
          ? before.rollbackHealthBuild
          : nextRollbackVersion === before.selectedVersion
          ? before.selectedHealthBuild
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
          attempt: this.#record.attempt,
          lastOutcome: {
            txId,
            request,
            status: "committed_pending_publish",
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
        return await this.#terminal(txId, request, terminalStatus, committed);
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
      if (error instanceof StorageFailure) {
        this.#volatileBlocked = true;
        this.#windows.clear();
        return {
          txId,
          request,
          status: "storage_failed",
          selectedVersion: this.#record.selectedVersion,
        };
      }
      if (stopped.errors.length || stopped.timedOut) {
        return await this.#quarantine(
          txId,
          request,
          "candidate-cleanup-failure",
        );
      }
      const episode = this.#record.attempt;
      const alreadyTried = new Set([
        ...(episode?.attemptedDigests ?? []),
        ...(episode?.rejectedDigests ?? []),
      ]);
      let fallbackVersion: FixtureVersion | null = null;
      if (
        before.desiredEnabled && before.selectedVersion &&
        before.selectedDigest &&
        this.#selectedEligible(before) &&
        !alreadyTried.has(before.selectedDigest)
      ) fallbackVersion = before.selectedVersion;
      else if (
        request.op === "startup_recovery" && this.#rollbackEligible(before) &&
        before.rollbackVersion && before.rollbackDigest &&
        !alreadyTried.has(before.rollbackDigest)
      ) fallbackVersion = before.rollbackVersion;
      if (fallbackVersion) {
        const fallbackDigest = M0_FIXTURES[fallbackVersion].digest;
        let fallback: M0FixtureValue;
        try {
          fallback = await this.#store.load(fallbackDigest, fallbackVersion);
        } catch (fallbackError) {
          if (fallbackError instanceof StorageFailure) {
            this.#volatileBlocked = true;
            return {
              txId,
              request,
              status: "storage_failed",
              selectedVersion: this.#record.selectedVersion,
            };
          }
          const episode = this.#record.attempt;
          const currentPending = this.#record.pending;
          if (
            episode && currentPending?.txId === txId &&
            !episode.attemptedDigests.includes(fallbackDigest) &&
            !episode.rejectedDigests.includes(fallbackDigest) &&
            episode.attemptedDigests.length + episode.rejectedDigests.length < 2
          ) {
            await this.#write({
              ...this.#record,
              pending: {
                ...currentPending,
                targetVersion: fallbackVersion,
                targetDigest: fallbackDigest,
                phase: "OLD_STOPPED",
              },
              attempt: {
                ...episode,
                targetDigest: fallbackDigest,
                rejectedDigests: [...episode.rejectedDigests, fallbackDigest],
              },
            });
          }
          return await this.#quarantine(
            txId,
            request,
            "recovery-fallback-preflight-rejected",
          );
        }
        return await this.#activate(
          txId,
          request,
          before,
          fallback,
          request.op === "startup_recovery" ? "succeeded" : "failed_restored",
        );
      }
      if (before.desiredEnabled) {
        return await this.#quarantine(
          txId,
          request,
          "recovery-attempt-exhausted",
        );
      }
      this.#windows.clear();
      return await this.#terminal(txId, request, "failed_restored", {
        ...before,
        desiredEnabled: false,
        pending: null,
        attempt: null,
      }, String(error));
    }
  }

  #archiveTerminalBeforeReplacement(record: M0Record, txId: string): M0Record {
    const previous = record.lastOutcome;
    if (
      !previous || previous.txId === txId || previous.status === "pending" ||
      previous.status === "committed_pending_publish"
    ) {
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
        base.lastOutcome.status !== "pending" &&
        base.lastOutcome.status !== "committed_pending_publish"
      ? [...base.recentOutcomes, base.lastOutcome].slice(-32)
      : base.recentOutcomes;
    const next = await this.#write({
      ...base,
      pending: null,
      attempt: status === "succeeded" || status === "failed_restored"
        ? null
        : base.attempt,
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
  shutdown(): Promise<DisposeReport>;
  shutdownForRestartForTests(): Promise<DisposeReport>;
  failNextWriteForTests(
    mode: "before_replace" | "after_replace",
    skip?: number,
  ): void;
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
  reopenExisting = false,
): Promise<M0TestHostHandle> {
  const suffix = Services.uuid.generateUUID().toString().replace(/[{}]/g, "")
    .toLowerCase();
  const root = rootOverride ?? PathUtils.join(TEST_ROOT, `m0-${suffix}`);
  const createdRoot = !(await IOUtils.exists(root));
  const host = await M0FeatureHost.open(
    root,
    experimentalOptIn,
    true,
    reopenExisting,
  );
  let cleanupPromise: Promise<DisposeReport> | null = null;
  return {
    root,
    host,
    shutdown: () => host.shutdown(),
    shutdownForRestartForTests: async () => {
      const report = await host.shutdown();
      if (report.errors.length === 0 && !report.timedOut) owners.delete(root);
      return report;
    },
    failNextWriteForTests: (mode, skip) =>
      host.setNextWriteFaultForTests(mode, skip),
    cleanup: () => {
      if (cleanupPromise) return cleanupPromise;
      cleanupPromise = (async () => {
        const report = await host.shutdown();
        if (
          createdRoot &&
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
