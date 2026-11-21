// SPDX-License-Identifier: MPL-2.0

import { type FixtureVersion, M0_FIXTURES } from "./M0Fixtures.sys.mts";
import {
  type Cleanup,
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
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
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

interface WindowReadyWaiter {
  resolve(entry: WindowEntry | null): void;
  reject(error: Error): void;
  timer: number;
}

interface WindowAttachGate {
  windowId: string;
  entered(): void;
  promise: Promise<void>;
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

function isM0WindowCandidate(window: Window): boolean {
  if (window.closed) return false;
  const documentElement = window.document.documentElement;
  return isEligibleM0MainWindowForTests(
    documentElement?.getAttribute("windowtype") ?? null,
    true,
    PrivateBrowsingUtils.isWindowPrivate(window),
    isM0WebPanelWindow(window),
  );
}

function isEligibleM0Window(window: Window): boolean {
  return isM0WindowCandidate(window) && (window as Window & {
        toolbar?: { visible?: boolean };
      }).toolbar?.visible === true;
}

function isM0DelayedStartupFinished(window: Window): boolean {
  return (window as Window & {
    gBrowserInit?: { delayedStartupFinished?: boolean };
  }).gBrowserInit?.delayedStartupFinished === true;
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
  readonly #windowEntries = new Map<Window, WindowEntry>();
  readonly #pendingStartupWindows = new Set<Window>();
  readonly #loadListeners = new Map<Window, EventListener>();
  readonly #readyWindows = new WeakSet<Window>();
  readonly #readyWaiters = new Map<Window, Set<WindowReadyWaiter>>();
  readonly #closeWaiters = new Map<Window, Set<() => void>>();
  readonly #windowAdmissionTasks = new Map<string, Promise<void>>();
  readonly #windowDetachments = new Map<string, Set<Promise<DisposeReport>>>();
  readonly #windowCleanupFailures = new Map<string, string[]>();
  readonly #windowObserver: nsIObserver = {
    observe: (subject: nsISupports | null, topic: string) => {
      if (!subject) return;
      const window = subject as unknown as Window;
      if (topic === "domwindowopened") this.#onWindowOpened(window);
      else if (topic === "browser-delayed-startup-finished") {
        this.#onWindowReady(window);
      } else if (topic === "domwindowclosed") this.#onWindowClosed(window);
    },
  };
  #record = defaultRecord();
  #running: M0ModuleHost | null = null;
  #epoch = Services.uuid.generateUUID().toString();
  #generation = 0;
  #revocationEpoch = 0;
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
  #windowObserverInstalled = false;
  #admissionsOpen = false;
  #publishedToken: string | null = null;
  #nextWindowAttachGate: WindowAttachGate | null = null;
  #nextWindowAttachCompletionGate: WindowAttachGate | null = null;
  #testAttachFailureWindowId: string | null = null;
  #testExcludeWindows = false;
  #testOnlyWindow: Window | null = null;

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
        host.#installWindowObservers();
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
        host.#removeWindowObservers();
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

  get windowLifecycleDiagnosticsForTests(): {
    observers: boolean;
    tracked: number;
    pendingStartup: number;
    readyWaiters: number;
    admissionTasks: number;
    pendingDetaches: number;
    cleanupFailures: number;
  } {
    return {
      observers: this.#windowObserverInstalled,
      tracked: this.#windowEntries.size,
      pendingStartup: this.#pendingStartupWindows.size,
      readyWaiters: [...this.#readyWaiters.values()].reduce(
        (count, waiters) => count + waiters.size,
        0,
      ),
      admissionTasks: this.#windowAdmissionTasks.size,
      pendingDetaches: [...this.#windowDetachments.values()].reduce(
        (count, pending) => count + pending.size,
        0,
      ),
      cleanupFailures: [...this.#windowCleanupFailures.values()].reduce(
        (count, failures) => count + failures.length,
        0,
      ),
    };
  }

  windowIdForTests(window: Window): string | null {
    return this.#windowEntries.get(window)?.id ?? null;
  }

  deferWindowAttachForTests(windowId: string): {
    entered: Promise<void>;
    release(): void;
  } {
    if (!this.#testWritesEnabled || this.#nextWindowAttachGate) {
      throw new Error("window attach gate unavailable");
    }
    let enter!: () => void;
    let release!: () => void;
    const entered = new Promise<void>((resolve) => enter = resolve);
    const promise = new Promise<void>((resolve) => release = resolve);
    this.#nextWindowAttachGate = { windowId, entered: enter, promise };
    return { entered, release };
  }

  deferWindowAttachCompletionForTests(windowId = "*"): {
    entered: Promise<void>;
    release(): void;
  } {
    if (
      !this.#testWritesEnabled || this.#nextWindowAttachCompletionGate
    ) throw new Error("window attach completion gate unavailable");
    let enter!: () => void;
    let release!: () => void;
    const entered = new Promise<void>((resolve) => enter = resolve);
    const promise = new Promise<void>((resolve) => release = resolve);
    this.#nextWindowAttachCompletionGate = {
      windowId,
      entered: enter,
      promise,
    };
    return { entered, release };
  }

  registerWindowCleanupForTests(windowId: string, cleanup: Cleanup): void {
    if (!this.#testWritesEnabled || !this.#running) {
      throw new Error("window cleanup registration unavailable");
    }
    this.#running.scope.child(windowId).register(cleanup);
  }

  failWindowAttachForTests(windowId = "*"): void {
    if (!this.#testWritesEnabled) {
      throw new Error("test attach failure disabled");
    }
    this.#testAttachFailureWindowId = windowId;
  }

  excludeWindowsForTests(): void {
    if (!this.#testWritesEnabled || this.#running) {
      throw new Error("window exclusion is unavailable");
    }
    this.#testExcludeWindows = true;
    for (const window of this.#windowEntries.keys()) this.#forgetWindow(window);
  }

  useOnlyWindowForTests(window: Window): void {
    if (!this.#testWritesEnabled || this.#running || window.closed) {
      throw new Error("window filter is unavailable");
    }
    this.#testExcludeWindows = false;
    this.#testOnlyWindow = window;
    for (const current of this.#windowEntries.keys()) {
      if (current !== window) this.#forgetWindow(current);
    }
    this.#onWindowOpened(window);
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
    this.#revocationEpoch += 1;
    this.#admissionsOpen = false;
    this.#removeWindowObservers();
    this.#windows.clear();
    this.#markerKeys.clear();
    this.#publishedToken = null;
    this.#shutdownPromise = (async () => {
      await this.#queue;
      await Promise.allSettled([...this.#windowAdmissionTasks.values()]);
      this.#activeToken = null;
      this.#publishedToken = null;
      const result = this.#running
        ? await this.#running.stop("host-shutdown")
        : { errors: [], timedOut: false };
      this.#running = null;
      this.#windows.clear();
      this.#markerKeys.clear();
      this.#windowEntries.clear();
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

  #assertOperationCurrent(operationEpoch: number): void {
    if (this.#closed) throw new Error("host shutdown");
    if (
      this.#volatileBlocked || this.#record.blockedReason ||
      this.#revocationEpoch !== operationEpoch
    ) throw new Error("quarantined");
  }

  #isCurrentGeneration(runtime: M0ModuleHost, token: string): boolean {
    return !this.#closed && !this.#volatileBlocked &&
      !this.#record.blockedReason && this.#running === runtime &&
      this.#activeToken === token;
  }

  #assertGenerationCurrent(
    operationEpoch: number,
    runtime: M0ModuleHost,
    token: string,
  ): void {
    this.#assertOperationCurrent(operationEpoch);
    if (!this.#isCurrentGeneration(runtime, token)) {
      throw new Error("generation revoked");
    }
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
    const operationEpoch = this.#revocationEpoch;
    this.#assertOperationCurrent(operationEpoch);
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
      this.#assertOperationCurrent(operationEpoch);
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
      this.#assertOperationCurrent(operationEpoch);
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
      const previousAdmission = this.#admissionsOpen;
      this.#admissionsOpen = false;
      const pending: PendingTransaction = {
        op: request.op,
        txId,
        request,
        before: snapshotRecord(before),
        targetVersion: before.selectedVersion,
        targetDigest: before.selectedDigest,
        phase: "PREPARED",
      };
      try {
        await this.#write({
          ...before,
          desiredEnabled: false,
          pending,
          attempt: null,
        });
      } catch (error) {
        if (!(error instanceof StorageFailure) || !error.uncertain) {
          this.#admissionsOpen = previousAdmission;
          if (previousAdmission) this.#scheduleWindowAdmissions();
        }
        throw error;
      }
      const stopped = this.#running
        ? await this.#running.stop("disable")
        : { errors: [], timedOut: false };
      this.#assertOperationCurrent(operationEpoch);
      this.#running = null;
      this.#activeToken = null;
      this.#publishedToken = null;
      this.#windows.clear();
      this.#markerKeys.clear();
      if (stopped.errors.length || stopped.timedOut) {
        return await this.#quarantine(txId, request, "disable-cleanup-failure");
      }
      const outcome = await this.#terminal(txId, request, "succeeded", {
        ...this.#record,
        desiredEnabled: false,
        pending: null,
        attempt: null,
      });
      this.#admissionsOpen = true;
      return outcome;
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
    if (
      request.op === "rollback" &&
      before.rollbackDigest === before.selectedDigest
    ) {
      throw new Error("rollback target is already selected");
    }
    if (request.op === "rollback" && !this.#rollbackEligible(before)) {
      throw new Error("rollback target lacks current-build health proof");
    }
    const fixture = await this.#store.ensure(targetVersion);
    this.#assertOperationCurrent(operationEpoch);
    if (request.op === "enable" || before.desiredEnabled) {
      return await this.#activate(
        txId,
        request,
        before,
        fixture,
        "succeeded",
        undefined,
        operationEpoch,
      );
    }
    if (request.op === "update" || request.op === "rollback") {
      const rollbackTarget = this.#healthyRollbackTarget(
        before,
        fixture.digest,
      );
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
      this.#assertOperationCurrent(operationEpoch);
      return await this.#terminal(txId, request, "succeeded", {
        ...this.#record,
        selectedVersion: fixture.version,
        selectedDigest: fixture.digest,
        selectedHealth: request.op === "rollback" ? "verified" : "unverified",
        selectedHealthBuild: request.op === "rollback"
          ? before.rollbackHealthBuild
          : null,
        rollbackVersion: rollbackTarget?.version ?? null,
        rollbackDigest: rollbackTarget?.digest ?? null,
        rollbackHealthBuild: rollbackTarget?.healthBuild ?? null,
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
      record.rollbackDigest !== record.selectedDigest &&
      record.rollbackDigest === M0_FIXTURES[record.rollbackVersion].digest &&
      record.rollbackHealthBuild?.hostBuildId === HOST_BUILD_ID &&
      record.rollbackHealthBuild.runtimeBuildId === Services.appinfo.appBuildID;
  }

  #healthyRollbackTarget(
    record: M0Record,
    selectedDigest: string,
  ): {
    version: FixtureVersion;
    digest: string;
    healthBuild: M0Record["rollbackHealthBuild"];
  } | null {
    if (
      record.selectedVersion && record.selectedDigest &&
      record.selectedDigest !== selectedDigest &&
      this.#selectedEligible(record)
    ) {
      return {
        version: record.selectedVersion,
        digest: record.selectedDigest,
        healthBuild: record.selectedHealthBuild,
      };
    }
    if (
      record.rollbackVersion && record.rollbackDigest &&
      record.rollbackDigest !== selectedDigest &&
      this.#rollbackEligible(record)
    ) {
      return {
        version: record.rollbackVersion,
        digest: record.rollbackDigest,
        healthBuild: record.rollbackHealthBuild,
      };
    }
    return null;
  }

  async #activate(
    txId: string,
    request: Descriptor,
    before: M0Record,
    fixture: M0FixtureValue,
    terminalStatus: "succeeded" | "failed_restored" = "succeeded",
    windowSnapshot?: readonly WindowEntry[],
    operationEpoch = this.#revocationEpoch,
  ): Promise<Outcome> {
    this.#assertOperationCurrent(operationEpoch);
    const verifiedFixture = fixture;
    const previousAdmission = this.#admissionsOpen;
    const previousPublishedToken = this.#publishedToken;
    this.#admissionsOpen = false;
    this.#publishedToken = null;
    const windows = windowSnapshot
      ? [...windowSnapshot]
      : this.#captureWindows();
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
      try {
        await this.#write({ ...before, pending, attempt: null });
        this.#assertOperationCurrent(operationEpoch);
      } catch (error) {
        if (
          (!(error instanceof StorageFailure) || !error.uncertain) &&
          this.#record.pending?.txId !== txId
        ) {
          this.#admissionsOpen = previousAdmission;
          this.#publishedToken = previousPublishedToken;
          if (previousAdmission && previousPublishedToken) {
            for (const [id, window] of this.#windows) {
              this.#publishWindowMarker({ id, window }, previousPublishedToken);
            }
            this.#scheduleWindowAdmissions();
          }
        }
        throw error;
      }
    }
    if (this.#closed) throw new Error("host shutdown");
    const old = this.#running;
    if (old) {
      this.#activeToken = null;
      const stopped = await old.stop("replacement");
      this.#assertOperationCurrent(operationEpoch);
      if (stopped.errors.length || stopped.timedOut) {
        if (this.#running === old) this.#running = null;
        return await this.#quarantine(txId, request, "old-cleanup-failure");
      }
      if (this.#running === old) this.#running = null;
    }
    this.#assertOperationCurrent(operationEpoch);
    this.#windows.clear();
    this.#markerKeys.clear();
    this.#publishedToken = null;
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
    this.#assertOperationCurrent(operationEpoch);
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
    this.#assertOperationCurrent(operationEpoch);
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
      this.#assertGenerationCurrent(operationEpoch, runtime, token);
      this.#evaluations += 1;
      m0PayloadEvaluations += 1;
      await runtime.start();
      this.#assertGenerationCurrent(operationEpoch, runtime, token);
      for (const entry of windows) {
        const attached = await this.#attachWindowEntry(
          entry,
          runtime,
          token,
          true,
        );
        this.#assertGenerationCurrent(operationEpoch, runtime, token);
        if (!attached && this.#isCurrentWindowEntry(entry)) {
          throw new Error("core window attach failed");
        }
      }
      await this.#awaitWindowCleanups(runtime, token);
      this.#assertGenerationCurrent(operationEpoch, runtime, token);
      pending = { ...pending, phase: "CANDIDATE_READY" };
      await this.#write({ ...this.#record, pending });
      this.#assertGenerationCurrent(operationEpoch, runtime, token);
      await this.#awaitWindowCleanups(runtime, token);
      this.#assertGenerationCurrent(operationEpoch, runtime, token);
      const build = {
        hostBuildId: HOST_BUILD_ID,
        runtimeBuildId: Services.appinfo.appBuildID,
      };
      this.#running = runtime;
      const startupFallback = request.op === "startup_recovery" &&
        before.selectedDigest !== fixture.digest;
      const rollbackTarget = startupFallback
        ? null
        : this.#healthyRollbackTarget(before, fixture.digest);
      let committed: M0Record;
      try {
        this.#assertGenerationCurrent(operationEpoch, runtime, token);
        committed = await this.#write({
          ...this.#archiveTerminalBeforeReplacement(before, txId),
          selectedVersion: fixture.version,
          selectedDigest: fixture.digest,
          desiredEnabled: true,
          selectedHealth: "verified",
          selectedHealthBuild: build,
          rollbackVersion: rollbackTarget?.version ?? null,
          rollbackDigest: rollbackTarget?.digest ?? null,
          rollbackHealthBuild: rollbackTarget?.healthBuild ?? null,
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
      this.#assertGenerationCurrent(operationEpoch, runtime, token);
      await this.#awaitWindowCleanups(runtime, token);
      this.#assertGenerationCurrent(operationEpoch, runtime, token);
      let outcome: Outcome;
      try {
        this.#publishedToken = token;
        for (const entry of windows) this.#publishWindowMarker(entry, token);
        outcome = await this.#terminal(
          txId,
          request,
          terminalStatus,
          committed,
        );
      } catch {
        return await this.#finalizationStorageFailure(
          txId,
          request,
          runtime,
          "finalization-storage-failure",
        );
      }
      await this.#awaitWindowCleanups(runtime, token);
      this.#assertGenerationCurrent(operationEpoch, runtime, token);
      this.#openWindowAdmissions(runtime, token);
      return outcome;
    } catch (error) {
      const ownedGeneration = this.#running === runtime &&
        this.#generation === generation;
      if (ownedGeneration && this.#activeToken === token) {
        this.#activeToken = null;
      }
      const stopped = await runtime.stop("candidate-failed");
      if (this.#running === runtime && this.#generation === generation) {
        this.#running = null;
        this.#windows.clear();
        this.#markerKeys.clear();
        if (this.#publishedToken === token) this.#publishedToken = null;
      }
      if (error instanceof StorageFailure) {
        this.#volatileBlocked = true;
        this.#revocationEpoch += 1;
        return {
          txId,
          request,
          status: "storage_failed",
          selectedVersion: this.#record.selectedVersion,
        };
      }
      if (
        !this.#closed && this.#windowCleanupFailures.get(token)?.length
      ) {
        return await this.#quarantine(
          txId,
          request,
          "candidate-window-cleanup-failure",
        );
      }
      if (stopped.errors.length || stopped.timedOut) {
        return await this.#quarantine(
          txId,
          request,
          "candidate-cleanup-failure",
        );
      }
      if (this.#closed) throw error;
      if (
        this.#revocationEpoch !== operationEpoch || this.#volatileBlocked ||
        this.#record.blockedReason
      ) {
        return {
          txId,
          request,
          status: "failed_quarantined",
          selectedVersion: this.#record.selectedVersion,
        };
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
          windows,
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
      this.#markerKeys.clear();
      const outcome = await this.#terminal(txId, request, "failed_restored", {
        ...before,
        desiredEnabled: false,
        pending: null,
        attempt: null,
      }, String(error));
      this.#admissionsOpen = true;
      return outcome;
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
    const generation = this.#generation;
    this.#revocationEpoch += 1;
    this.#volatileBlocked = true;
    if (this.#running === runtime) {
      this.#activeToken = null;
      this.#admissionsOpen = false;
      this.#publishedToken = null;
    }
    await runtime.stop(reason);
    if (this.#running === runtime && this.#generation === generation) {
      this.#running = null;
      this.#windows.clear();
      this.#markerKeys.clear();
    }
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

  #installWindowObservers(): void {
    if (this.#windowObserverInstalled) return;
    const topics = [
      "domwindowopened",
      "browser-delayed-startup-finished",
      "domwindowclosed",
    ];
    let registered = 0;
    try {
      for (const topic of topics) {
        Services.obs.addObserver(this.#windowObserver, topic);
        registered += 1;
      }
      this.#windowObserverInstalled = true;
    } catch (error) {
      for (const topic of topics.slice(0, registered)) {
        Services.obs.removeObserver(this.#windowObserver, topic);
      }
      throw error;
    }
    const windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      this.#onWindowOpened(windows.getNext() as unknown as Window);
    }
  }

  #removeWindowObservers(): void {
    if (this.#windowObserverInstalled) {
      for (
        const topic of [
          "domwindowopened",
          "browser-delayed-startup-finished",
          "domwindowclosed",
        ]
      ) {
        Services.obs.removeObserver(this.#windowObserver, topic);
      }
      this.#windowObserverInstalled = false;
    }
    for (const [window, listener] of this.#loadListeners) {
      window.removeEventListener("load", listener);
    }
    this.#loadListeners.clear();
    this.#pendingStartupWindows.clear();
    for (const window of this.#readyWaiters.keys()) {
      this.#resolveWindowReady(window, null);
    }
    for (const window of this.#closeWaiters.keys()) {
      this.#signalWindowClosed(window);
    }
    this.#windowEntries.clear();
    this.#testOnlyWindow = null;
  }

  #onWindowOpened(window: Window): void {
    if (
      this.#closed || this.#testExcludeWindows || window.closed ||
      (this.#testOnlyWindow && this.#testOnlyWindow !== window) ||
      this.#pendingStartupWindows.has(window)
    ) {
      return;
    }
    if (isM0DelayedStartupFinished(window)) {
      this.#onWindowReady(window);
      return;
    }
    this.#pendingStartupWindows.add(window);
    if (window.document?.readyState === "complete") {
      this.#considerLoadedWindow(window);
      return;
    }
    const listener: EventListener = () => this.#considerLoadedWindow(window);
    this.#loadListeners.set(window, listener);
    window.addEventListener("load", listener, { once: true });
  }

  #considerLoadedWindow(window: Window): void {
    const listener = this.#loadListeners.get(window);
    if (listener) {
      window.removeEventListener("load", listener);
      this.#loadListeners.delete(window);
    }
    if (this.#closed || window.closed) {
      this.#forgetWindow(window);
      return;
    }
    if (!isM0WindowCandidate(window)) {
      this.#pendingStartupWindows.delete(window);
      this.#resolveWindowReady(window, null);
      return;
    }
    this.#ensureWindowEntry(window);
    if (isM0DelayedStartupFinished(window)) this.#onWindowReady(window);
  }

  #onWindowReady(window: Window): void {
    if (
      this.#closed || this.#testExcludeWindows || window.closed ||
      (this.#testOnlyWindow && this.#testOnlyWindow !== window)
    ) {
      this.#forgetWindow(window);
      return;
    }
    const listener = this.#loadListeners.get(window);
    if (listener) {
      window.removeEventListener("load", listener);
      this.#loadListeners.delete(window);
    }
    this.#pendingStartupWindows.delete(window);
    if (!isEligibleM0Window(window)) {
      this.#forgetWindow(window);
      return;
    }
    const entry = this.#ensureWindowEntry(window);
    if (!entry) {
      this.#resolveWindowReady(window, null);
      return;
    }
    this.#readyWindows.add(window);
    this.#resolveWindowReady(window, entry);
    if (this.#admissionsOpen) this.#scheduleWindowAdmission(entry);
  }

  #ensureWindowEntry(window: Window): WindowEntry | null {
    if (window.closed || !isM0WindowCandidate(window)) return null;
    const existing = this.#windowEntries.get(window);
    if (existing) return existing;
    const entry = {
      id: Services.uuid.generateUUID().toString(),
      window,
    };
    this.#windowEntries.set(window, entry);
    return entry;
  }

  #waitForWindowReady(entry: WindowEntry): Promise<WindowEntry | null> {
    const window = entry.window;
    let ready: Promise<WindowEntry | null>;
    if (window.closed || this.#windowEntries.get(window) !== entry) {
      ready = Promise.resolve(null);
    } else if (this.#readyWindows.has(window)) {
      ready = Promise.resolve(entry);
    } else {
      this.#onWindowOpened(window);
      if (this.#readyWindows.has(window)) {
        ready = Promise.resolve(entry);
      } else {
        ready = new Promise<WindowEntry | null>((resolve, reject) => {
          const waiters = this.#readyWaiters.get(window) ?? new Set();
          const waiter: WindowReadyWaiter = {
            resolve,
            reject,
            timer: setTimeout(() => {
              this.#removeReadyWaiter(window, waiter);
              if (window.closed || this.#windowEntries.get(window) !== entry) {
                resolve(null);
              } else {
                reject(new Error("browser delayed startup timed out"));
              }
            }, 30_000),
          };
          waiters.add(waiter);
          this.#readyWaiters.set(window, waiters);
        });
      }
    }
    return ready.then(async (resolved) => {
      if (!resolved) return null;
      const gate = this.#nextWindowAttachGate?.windowId === entry.id
        ? this.#nextWindowAttachGate
        : null;
      if (!gate) return resolved;
      this.#nextWindowAttachGate = null;
      gate.entered();
      if (await this.#waitForCloseOr(gate.promise, window)) return null;
      return this.#isCurrentWindowEntry(entry) ? entry : null;
    });
  }

  #removeReadyWaiter(window: Window, waiter: WindowReadyWaiter): void {
    clearTimeout(waiter.timer);
    const waiters = this.#readyWaiters.get(window);
    waiters?.delete(waiter);
    if (waiters?.size === 0) this.#readyWaiters.delete(window);
  }

  #resolveWindowReady(window: Window, entry: WindowEntry | null): void {
    const waiters = this.#readyWaiters.get(window);
    if (!waiters) return;
    this.#readyWaiters.delete(window);
    for (const waiter of waiters) {
      clearTimeout(waiter.timer);
      waiter.resolve(entry);
    }
  }

  #signalWindowClosed(window: Window): void {
    const waiters = this.#closeWaiters.get(window);
    if (!waiters) return;
    this.#closeWaiters.delete(window);
    for (const resolve of waiters) resolve();
  }

  #onWindowClosed(window: Window): void {
    const entry = this.#windowEntries.get(window) ?? null;
    const wasAttached = entry !== null &&
      this.#windows.get(entry.id) === window;
    if (this.#testOnlyWindow === window) this.#testOnlyWindow = null;
    this.#forgetWindow(window);
    if (!entry || !wasAttached) return;
    const runtime = this.#running;
    const token = this.#activeToken;
    const generation = this.#generation;
    if (!runtime) return;
    void this.#detachClosedWindow(entry.id, runtime, token, generation);
  }

  #detachClosedWindow(
    windowId: string,
    runtime: M0ModuleHost,
    token: string | null,
    generation: number,
  ): Promise<DisposeReport> {
    const detached = runtime.detachWindow(windowId).then(
      (report) => report,
      (error) => ({ errors: [String(error)], timedOut: false }),
    );
    if (!token) return detached;
    let pending = this.#windowDetachments.get(token);
    if (!pending) {
      pending = new Set();
      this.#windowDetachments.set(token, pending);
    }
    const tracked = detached.then((report) => {
      if (report.errors.length || report.timedOut) {
        const failures = this.#windowCleanupFailures.get(token) ?? [];
        if (failures.length < 8) {
          failures.push(
            [
              ...report.errors,
              ...(report.timedOut ? ["cleanup timed out"] : []),
            ]
              .join("; ").slice(0, 256),
          );
          this.#windowCleanupFailures.set(token, failures);
        }
        if (
          this.#running === runtime && this.#generation === generation &&
          this.#publishedToken === token
        ) {
          void this.#quarantineActiveGeneration(
            runtime,
            token,
            "window-cleanup-failure",
          );
        }
      }
      return report;
    }).finally(() => {
      pending?.delete(tracked);
      if (pending?.size === 0) this.#windowDetachments.delete(token);
    });
    pending.add(tracked);
    return tracked;
  }

  async #awaitWindowCleanups(
    runtime: M0ModuleHost,
    token: string,
  ): Promise<void> {
    while (this.#running === runtime && this.#activeToken === token) {
      const pending = [...(this.#windowDetachments.get(token) ?? [])];
      if (pending.length === 0) break;
      await Promise.all(pending);
    }
    if (this.#windowCleanupFailures.get(token)?.length) {
      throw new Error("window cleanup failed before activation finalized");
    }
  }

  #forgetWindow(window: Window): void {
    this.#pendingStartupWindows.delete(window);
    const listener = this.#loadListeners.get(window);
    if (listener) {
      window.removeEventListener("load", listener);
      this.#loadListeners.delete(window);
    }
    this.#resolveWindowReady(window, null);
    this.#signalWindowClosed(window);
    const entry = this.#windowEntries.get(window);
    if (!entry) return;
    this.#windowEntries.delete(window);
    this.#windows.delete(entry.id);
    for (const key of this.#markerKeys) {
      if (key.endsWith(`:${entry.id}`)) this.#markerKeys.delete(key);
    }
  }

  #captureWindows(): WindowEntry[] {
    if (this.#testExcludeWindows) return [];
    const entries: WindowEntry[] = [];
    const enumerator = Services.wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      const window = enumerator.getNext() as unknown as Window;
      if (this.#testOnlyWindow && this.#testOnlyWindow !== window) continue;
      if (!isEligibleM0Window(window)) continue;
      const entry = this.#ensureWindowEntry(window);
      if (!entry) continue;
      if (isM0DelayedStartupFinished(window)) this.#readyWindows.add(window);
      else this.#onWindowOpened(window);
      entries.push(entry);
    }
    return entries;
  }

  #isCurrentWindowEntry(entry: WindowEntry): boolean {
    return !this.#closed && !this.#testExcludeWindows && !entry.window.closed &&
      (!this.#testOnlyWindow || this.#testOnlyWindow === entry.window) &&
      this.#windowEntries.get(entry.window) === entry &&
      isEligibleM0Window(entry.window);
  }

  #isCurrentWindowAttachment(
    entry: WindowEntry,
    runtime: M0ModuleHost,
    token: string,
    initial: boolean,
  ): boolean {
    return this.#isCurrentGeneration(runtime, token) &&
      (initial || this.#admissionsOpen) && this.#isCurrentWindowEntry(entry);
  }

  async #attachWindowEntry(
    entry: WindowEntry,
    runtime: M0ModuleHost,
    token: string,
    initial = false,
  ): Promise<boolean> {
    const ready = await this.#waitForWindowReady(entry);
    if (
      !ready || !this.#isCurrentWindowAttachment(ready, runtime, token, initial)
    ) return false;
    this.#windows.set(entry.id, entry.window);
    if (
      this.#testAttachFailureWindowId === entry.id ||
      this.#testAttachFailureWindowId === "*"
    ) {
      this.#testAttachFailureWindowId = null;
      if (this.#isCurrentWindowAttachment(entry, runtime, token, initial)) {
        this.#windows.delete(entry.id);
      }
      throw new Error("injected browser window attach failure");
    }
    try {
      const attached = await runtime.attachWindow(entry.id);
      const gate = this.#nextWindowAttachCompletionGate &&
          (this.#nextWindowAttachCompletionGate.windowId === "*" ||
            this.#nextWindowAttachCompletionGate.windowId === entry.id)
        ? this.#nextWindowAttachCompletionGate
        : null;
      if (gate) {
        this.#nextWindowAttachCompletionGate = null;
        gate.entered();
        await gate.promise;
      }
      if (!this.#isCurrentWindowAttachment(entry, runtime, token, initial)) {
        return false;
      }
      if (!attached) throw new Error("core window attach failed");
      if (this.#publishedToken === token) {
        this.#publishWindowMarker(entry, token);
      }
      return true;
    } catch (error) {
      if (!this.#isCurrentWindowAttachment(entry, runtime, token, initial)) {
        return false;
      }
      if (this.#windows.get(entry.id) === entry.window) {
        this.#windows.delete(entry.id);
      }
      throw error;
    }
  }

  #waitForCloseOr(promise: Promise<void>, window: Window): Promise<boolean> {
    let resolveClosed!: () => void;
    const closed = new Promise<void>((resolve) => resolveClosed = resolve);
    const waiters = this.#closeWaiters.get(window) ?? new Set();
    waiters.add(resolveClosed);
    this.#closeWaiters.set(window, waiters);
    return Promise.race([promise.then(() => false), closed.then(() => true)])
      .finally(() => {
        const current = this.#closeWaiters.get(window);
        current?.delete(resolveClosed);
        if (current?.size === 0) this.#closeWaiters.delete(window);
      });
  }

  #publishWindowMarker(entry: WindowEntry, token: string): void {
    const runtime = this.#running;
    if (
      !runtime || !this.#isCurrentGeneration(runtime, token) ||
      this.#publishedToken !== token ||
      this.#windows.get(entry.id) !== entry.window || entry.window.closed
    ) {
      return;
    }
    const root = entry.window.document.documentElement;
    const attribute = "data-floorp-hotfix-m0";
    if (root.getAttribute(attribute) === `${token}:staged`) {
      root.setAttribute(attribute, token);
    }
  }

  #scheduleWindowAdmissions(): void {
    for (const entry of this.#windowEntries.values()) {
      this.#scheduleWindowAdmission(entry);
    }
  }

  #scheduleWindowAdmission(entry: WindowEntry): void {
    const runtime = this.#running;
    const token = this.#activeToken;
    if (
      !this.#admissionsOpen || this.#closed || !runtime || !token ||
      this.#publishedToken !== token || this.#windows.has(entry.id) ||
      this.#windowAdmissionTasks.has(entry.id)
    ) return;
    const task = this.#attachWindowEntry(entry, runtime, token).then(() => {})
      .catch(
        () => {
          if (
            this.#admissionsOpen && this.#running === runtime &&
            this.#activeToken === token && this.#publishedToken === token
          ) {
            return this.#quarantineActiveGeneration(
              runtime,
              token,
              "postcommit-window-attach-failure",
            );
          }
        },
      ).finally(() => {
        if (this.#windowAdmissionTasks.get(entry.id) === task) {
          this.#windowAdmissionTasks.delete(entry.id);
        }
      });
    this.#windowAdmissionTasks.set(entry.id, task);
  }

  #openWindowAdmissions(runtime: M0ModuleHost, token: string): void {
    if (
      this.#closed || this.#volatileBlocked || this.#running !== runtime ||
      this.#activeToken !== token || this.#publishedToken !== token
    ) return;
    this.#admissionsOpen = true;
    this.#scheduleWindowAdmissions();
  }

  async #quarantineActiveGeneration(
    runtime: M0ModuleHost,
    token: string,
    reason: string,
  ): Promise<void> {
    if (
      this.#running !== runtime || this.#activeToken !== token ||
      this.#publishedToken !== token
    ) return;
    const generation = this.#generation;
    this.#revocationEpoch += 1;
    this.#admissionsOpen = false;
    this.#activeToken = null;
    this.#publishedToken = null;
    this.#volatileBlocked = true;
    const stopped = await runtime.stop(reason);
    if (this.#running !== runtime || this.#generation !== generation) return;
    this.#running = null;
    this.#windows.clear();
    this.#markerKeys.clear();
    const durableReason = stopped.errors.length || stopped.timedOut
      ? `${reason}:cleanup-failure`
      : reason;
    try {
      await this.#enqueue(async () => {
        const record = this.#record;
        const pending = record.pending;
        const outcome = record.lastOutcome;
        if (pending) {
          await this.#terminal(
            pending.txId,
            pending.request,
            "failed_quarantined",
            { ...record, blockedReason: durableReason },
          );
        } else if (outcome?.status === "committed_pending_publish") {
          await this.#terminal(
            outcome.txId,
            outcome.request,
            "failed_quarantined",
            { ...record, blockedReason: durableReason },
          );
        } else if (!record.blockedReason) {
          await this.#write({ ...record, blockedReason: durableReason });
        }
      });
    } catch {
      this.#volatileBlocked = true;
    }
  }

  #broker(
    epoch: string,
    generation: number,
    json: string,
    token: string,
  ): string {
    const runtime = this.#running;
    if (
      this.#closed || this.#activeToken !== token || epoch !== this.#epoch ||
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
    const stagedValue = `${token}:staged`;
    const markerValue = this.#publishedToken === token ? token : stagedValue;
    this.#markerKeys.add(markerKey);
    const cleanup = () => {
      try {
        if (window.closed) return;
        if (
          root.getAttribute(attribute) === token ||
          root.getAttribute(attribute) === stagedValue
        ) {
          if (previous === null) root.removeAttribute(attribute);
          else root.setAttribute(attribute, previous);
        }
      } catch (error) {
        if (!window.closed) throw error;
      } finally {
        this.#markerKeys.delete(markerKey);
      }
    };
    try {
      root.setAttribute(attribute, markerValue);
      scope.register(cleanup);
    } catch (error) {
      this.#markerKeys.delete(markerKey);
      try {
        if (
          !window.closed && (
            root.getAttribute(attribute) === token ||
            root.getAttribute(attribute) === stagedValue
          )
        ) {
          if (previous === null) root.removeAttribute(attribute);
          else root.setAttribute(attribute, previous);
        }
      } catch (restoreError) {
        if (!window.closed) throw restoreError;
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
    this.#revocationEpoch += 1;
    this.#activeToken = null;
    this.#admissionsOpen = false;
    this.#publishedToken = null;
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
  deferWindowAttachForTests(windowId: string): {
    entered: Promise<void>;
    release(): void;
  };
  deferWindowAttachCompletionForTests(windowId?: string): {
    entered: Promise<void>;
    release(): void;
  };
  registerWindowCleanupForTests(windowId: string, cleanup: Cleanup): void;
  failWindowAttachForTests(windowId: string): void;
  excludeWindowsForTests(): void;
  useOnlyWindowForTests(window: Window): void;
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
    deferWindowAttachForTests: (windowId) =>
      host.deferWindowAttachForTests(windowId),
    deferWindowAttachCompletionForTests: (windowId) =>
      host.deferWindowAttachCompletionForTests(windowId),
    registerWindowCleanupForTests: (windowId, cleanup) =>
      host.registerWindowCleanupForTests(windowId, cleanup),
    failWindowAttachForTests: (windowId) =>
      host.failWindowAttachForTests(windowId),
    excludeWindowsForTests: () => host.excludeWindowsForTests(),
    useOnlyWindowForTests: (window) => host.useOnlyWindowForTests(window),
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
