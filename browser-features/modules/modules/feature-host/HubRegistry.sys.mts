// SPDX-License-Identifier: MPL-2.0

import type {
  HubClientConnection,
  HubClientDiscoveryAdapter,
  HubGenerationLease,
  HubHostClaim,
  HubLiveDocument,
  HubOwner,
  HubPageDescriptor,
  HubRegistryDiagnosticsForTests,
  HubSnapshot,
  HubSnapshotAcknowledgement,
  HubSnapshotEntry,
} from "./HubRegistryTypes.ts";
import {
  HubDescriptorValidationError,
  isHubIdentifier,
  parseHubPageDescriptor as parseValidatedHubPageDescriptor,
} from "./HubPageDescriptorValidation.ts";

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const MAX_SNAPSHOT_BYTES = 131_072;
const MAX_ENTRIES = 32;
const MAX_ENTRIES_PER_OWNER = 16;
const DEFAULT_PUBLICATION_TIMEOUT_MS = 2_000;
const forbiddenKeys = new Set(["__proto__", "prototype", "constructor"]);
const hostClaims = new Map<
  string,
  { registry: HubRegistry; hostEpoch: string; claim: HubHostClaim }
>();

function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null &&
    !Array.isArray(value);
}

function hasExactKeys(
  value: Record<string, unknown>,
  required: readonly string[],
  optional: readonly string[] = [],
): boolean {
  const keys = Object.keys(value);
  return keys.every((key) =>
    !forbiddenKeys.has(key) &&
    (required.includes(key) || optional.includes(key))
  ) && required.every((key) => Object.hasOwn(value, key));
}

export class HubRegistryError extends Error {
  constructor(readonly code: string) {
    super(code);
    this.name = "HubRegistryError";
  }
}

export function parseHubPageDescriptor(
  json: string,
  featureId: string,
): HubPageDescriptor {
  try {
    return parseValidatedHubPageDescriptor(json, featureId);
  } catch (error) {
    if (error instanceof HubDescriptorValidationError) {
      throw new HubRegistryError(error.code);
    }
    throw error;
  }
}

function parseAcknowledgement(json: string): HubSnapshotAcknowledgement | null {
  if (new TextEncoder().encode(json).byteLength > 1_024) return null;
  let value: unknown;
  try {
    value = JSON.parse(json);
  } catch {
    return null;
  }
  if (
    !isObject(value) ||
    !hasExactKeys(value, ["type", "epoch", "hubRevision"]) ||
    value.type !== "snapshot-ack" || typeof value.epoch !== "string" ||
    !Number.isSafeInteger(value.hubRevision) ||
    (value.hubRevision as number) < 0
  ) return null;
  return value as unknown as HubSnapshotAcknowledgement;
}

function sameOwner(left: HubOwner, right: HubOwner): boolean {
  return left.featureId === right.featureId &&
    left.hostEpoch === right.hostEpoch &&
    left.generation === right.generation;
}

interface LeaseState {
  owner: HubOwner;
  claim: HubHostClaim;
  entries: Map<string, HubSnapshotEntry>;
  sealed: boolean;
  published: boolean;
  disposed: boolean;
}

interface AckWaiter {
  document: HubLiveDocument;
  connection: HubClientConnection | null;
  epoch: string;
  revision: number;
  settled: boolean;
  owner: HubOwner | null;
  claim: HubHostClaim | null;
  absentFeatureId?: string;
  resolve(): void;
  reject(error: Error): void;
}

export class CoreOnlyHubClientDiscovery implements HubClientDiscoveryAdapter {
  discoverLiveHubDocuments(
    _windowIds: readonly string[],
  ): readonly HubLiveDocument[] {
    return [];
  }
}

export interface HubRegistryOptions {
  epoch?: string;
  discovery?: HubClientDiscoveryAdapter;
  reservedRoutes?: readonly string[];
}

export class HubRegistry {
  readonly #epoch: string;
  #discovery: HubClientDiscoveryAdapter;
  #reservedRoutes: ReadonlySet<string>;
  readonly #leases = new Map<HubGenerationLease, LeaseState>();
  readonly #claims = new Map<string, HubHostClaim>();
  readonly #published = new Map<string, HubSnapshotEntry>();
  readonly #activeOwners = new Map<string, HubOwner>();
  readonly #clients = new Map<string, HubClientConnection>();
  readonly #clientSubscriptions = new Map<HubClientConnection, () => void>();
  readonly #latestDelivered = new Map<
    HubClientConnection,
    { epoch: string; revision: number }
  >();
  readonly #renderedAcknowledgement = new Map<
    HubClientConnection,
    { epoch: string; revision: number }
  >();
  readonly #highestGeneration = new WeakMap<HubHostClaim, number>();
  readonly #waiters = new Set<AckWaiter>();
  #revision = 0;
  #pendingPublicationTimers = 0;
  #configurationLocked = false;

  constructor(options: HubRegistryOptions = {}) {
    this.#epoch = options.epoch ?? Services.uuid.generateUUID().toString();
    this.#discovery = options.discovery ?? new CoreOnlyHubClientDiscovery();
    this.#reservedRoutes = new Set(options.reservedRoutes ?? []);
    if (!this.#epoch || this.#epoch.length > 128) {
      throw new HubRegistryError("invalid hub epoch");
    }
  }

  setDiscoveryAdapter(adapter: HubClientDiscoveryAdapter): void {
    if (this.#discovery === adapter) return;
    if (
      this.#configurationLocked || this.#claims.size || this.#clients.size ||
      this.#waiters.size ||
      this.#leases.size || this.#published.size || this.#activeOwners.size
    ) {
      throw new HubRegistryError("HUB discovery must be configured before use");
    }
    this.#discovery = adapter;
  }

  isUsingDiscoveryAdapterForTests(
    adapter: HubClientDiscoveryAdapter,
  ): boolean {
    return this.#discovery === adapter;
  }

  setReservedRoutes(routes: readonly string[]): void {
    const nextRoutes = new Set(routes);
    if (
      nextRoutes.size === this.#reservedRoutes.size &&
      [...nextRoutes].every((route) => this.#reservedRoutes.has(route))
    ) return;
    if (
      this.#configurationLocked || this.#claims.size || this.#clients.size ||
      this.#waiters.size ||
      this.#leases.size || this.#published.size || this.#activeOwners.size
    ) {
      throw new HubRegistryError("HUB routes must be configured before use");
    }
    this.#reservedRoutes = nextRoutes;
  }

  getSnapshotForTests(): HubSnapshot {
    return JSON.parse(JSON.stringify(this.#snapshot())) as HubSnapshot;
  }

  get diagnosticsForTests(): HubRegistryDiagnosticsForTests {
    let stagedEntries = 0;
    for (const lease of this.#leases.values()) {
      if (!lease.disposed && !lease.published) {
        stagedEntries += lease.entries.size;
      }
    }
    return {
      hubRevision: this.#revision,
      stagedEntries,
      publishedEntries: this.#published.size,
      claims: this.#claims.size,
      leases: this.#leases.size,
      clients: this.#clients.size,
      pendingAcknowledgements: this.#waiters.size,
      pendingTimers: this.#pendingPublicationTimers,
    };
  }

  claimHost(featureId: string, hostEpoch: string): HubHostClaim {
    if (!isHubIdentifier(featureId) || !hostEpoch || hostEpoch.length > 128) {
      throw new HubRegistryError("invalid hub host claim");
    }
    if (hostClaims.has(featureId)) {
      throw new HubRegistryError("feature owner already claimed");
    }
    this.#configurationLocked = true;
    const claim: HubHostClaim = Object.freeze({
      featureId,
      hostEpoch,
      release: () => this.#releaseHost(claim),
    });
    hostClaims.set(featureId, { registry: this, hostEpoch, claim });
    this.#claims.set(featureId, claim);
    return claim;
  }

  beginGeneration(owner: HubOwner): HubGenerationLease {
    const claim = this.#claims.get(owner.featureId);
    if (
      !claim || claim.hostEpoch !== owner.hostEpoch ||
      !Number.isSafeInteger(owner.generation) || owner.generation < 1
    ) throw new HubRegistryError("hub owner is not claimed");
    const highestGeneration = this.#highestGeneration.get(claim) ?? 0;
    if (owner.generation <= highestGeneration) {
      throw new HubRegistryError("hub generation is not monotonic");
    }
    this.#highestGeneration.set(claim, owner.generation);
    const frozenOwner = Object.freeze({ ...owner });
    const state: LeaseState = {
      owner: frozenOwner,
      claim,
      entries: new Map(),
      sealed: false,
      published: false,
      disposed: false,
    };
    const lease: HubGenerationLease = Object.freeze({
      owner: frozenOwner,
      registerDescriptor: (json: string) =>
        this.#registerDescriptor(lease, json),
      dispose: () => this.#disposeGeneration(lease),
    });
    this.#leases.set(lease, state);
    return lease;
  }

  sealGeneration(lease: HubGenerationLease): void {
    const state = this.#leases.get(lease);
    if (!state || state.disposed || state.sealed) {
      throw new HubRegistryError("hub generation cannot be sealed");
    }
    state.sealed = true;
  }

  captureW0(windowIds: readonly string[]): readonly HubLiveDocument[] {
    const documentsInW0: HubLiveDocument[] = [];
    const windows = new Set(windowIds);
    const documentIds = new Set<string>();
    if (
      windows.size !== windowIds.length ||
      [...windows].some((windowId) => !windowId || windowId.length > 128)
    ) throw new HubRegistryError("invalid W0 window set");
    this.#configurationLocked = true;
    const documents = this.#discovery.discoverLiveHubDocuments([...windowIds]);
    for (const document of documents) {
      if (
        !document.documentId || document.documentId.length > 128 ||
        documentIds.has(document.documentId) || !windows.has(document.windowId)
      ) throw new HubRegistryError("invalid live HUB document in W0");
      documentIds.add(document.documentId);
      if (!document.isLive()) continue;
      const connection = document.ensureParentClient();
      if (connection) {
        if (connection.windowId !== document.windowId) {
          throw new HubRegistryError("HUB client window identity mismatch");
        }
        if (!connection.isClosed()) this.registerClient(connection);
      }
      documentsInW0.push(document);
    }
    return Object.freeze([...documentsInW0]);
  }

  registerClient(connection: HubClientConnection): void {
    if (connection.isClosed()) {
      throw new HubRegistryError("HUB client is closed");
    }
    if (
      !connection.clientId || !connection.connectionId ||
      !connection.windowId ||
      connection.clientId.length > 128 || connection.connectionId.length > 128
    ) throw new HubRegistryError("invalid HUB client identity");
    this.#configurationLocked = true;
    const previous = this.#clients.get(connection.connectionId);
    if (previous && previous !== connection) this.removeClient(previous);
    this.#clients.set(connection.connectionId, connection);
    if (!this.#clientSubscriptions.has(connection)) {
      this.#clientSubscriptions.set(
        connection,
        connection.onClosed(() => this.removeClient(connection)),
      );
    }
    const snapshot = this.#snapshot();
    const delivered = this.#latestDelivered.get(connection);
    if (
      delivered?.epoch !== snapshot.epoch ||
      delivered.revision !== snapshot.hubRevision
    ) this.#sendSnapshot(connection, snapshot);
  }

  removeClient(connection: HubClientConnection): void {
    if (this.#clients.get(connection.connectionId) === connection) {
      this.#clients.delete(connection.connectionId);
    }
    this.#latestDelivered.delete(connection);
    this.#renderedAcknowledgement.delete(connection);
    const unsubscribe = this.#clientSubscriptions.get(connection);
    if (unsubscribe) {
      this.#clientSubscriptions.delete(connection);
      unsubscribe();
    }
    for (const waiter of [...this.#waiters]) {
      if (waiter.connection === connection) waiter.connection = null;
    }
  }

  acknowledge(connection: HubClientConnection, json: string): boolean {
    const acknowledgement = parseAcknowledgement(json);
    const delivered = this.#latestDelivered.get(connection);
    if (
      !acknowledgement ||
      this.#clients.get(connection.connectionId) !== connection ||
      connection.isClosed() || !delivered ||
      delivered.epoch !== acknowledgement.epoch ||
      delivered.revision !== acknowledgement.hubRevision ||
      acknowledgement.epoch !== this.#epoch
    ) return false;
    this.#renderedAcknowledgement.set(connection, {
      epoch: acknowledgement.epoch,
      revision: acknowledgement.hubRevision,
    });
    for (const waiter of [...this.#waiters]) {
      if (
        waiter.connection === connection &&
        waiter.epoch === acknowledgement.epoch &&
        waiter.revision <= acknowledgement.hubRevision
      ) {
        if (this.#isWaitCurrent(waiter)) waiter.resolve();
        else waiter.reject(new HubRegistryError("publication superseded"));
      }
    }
    return true;
  }

  async publishGeneration(
    lease: HubGenerationLease,
    w0: readonly HubLiveDocument[],
    signal: AbortSignal,
    timeoutMs = DEFAULT_PUBLICATION_TIMEOUT_MS,
  ): Promise<HubSnapshot> {
    const deadline = this.#preparePublication(signal, timeoutMs);
    const state = this.#leases.get(lease);
    if (!state || state.disposed || !state.sealed) {
      throw new HubRegistryError("hub generation is not ready to publish");
    }
    if (
      this.#claims.get(state.owner.featureId) !== state.claim ||
      state.claim.hostEpoch !== state.owner.hostEpoch
    ) {
      throw new HubRegistryError("hub host claim revoked");
    }
    if (this.#highestGeneration.get(state.claim) !== state.owner.generation) {
      throw new HubRegistryError("stale HUB generation publication");
    }
    const next = new Map<string, HubSnapshotEntry>();
    for (const [key, entry] of this.#published) {
      if (entry.featureId !== state.owner.featureId) next.set(key, entry);
    }
    for (const entry of state.entries.values()) {
      next.set(`${entry.featureId}/${entry.localId}`, entry);
    }
    this.#assertUniqueRoutes(next);
    if (next.size > MAX_ENTRIES) {
      throw new HubRegistryError("hub entry limit exceeded");
    }
    const previous = new Map(this.#published);
    const previousOwner = this.#activeOwners.get(state.owner.featureId);
    const previousRevision = this.#revision;
    this.#published.clear();
    for (const [key, entry] of next) this.#published.set(key, entry);
    state.published = true;
    this.#activeOwners.set(state.owner.featureId, state.owner);
    if (previousOwner && !sameOwner(previousOwner, state.owner)) {
      this.#rejectOwnerWaiters(previousOwner);
    }
    this.#rejectRemovalWaiters(state.owner.featureId);
    this.#revision += 1;
    const snapshot = this.#snapshot();
    const json = JSON.stringify({ type: "snapshot", snapshot });
    if (new TextEncoder().encode(json).byteLength > MAX_SNAPSHOT_BYTES) {
      this.#published.clear();
      for (const [key, entry] of previous) this.#published.set(key, entry);
      if (previousOwner) {
        this.#activeOwners.set(state.owner.featureId, previousOwner);
      } else this.#activeOwners.delete(state.owner.featureId);
      this.#revision = previousRevision;
      state.published = false;
      throw new HubRegistryError("hub snapshot too large");
    }
    this.#broadcastSnapshot(snapshot, json);
    return await this.#awaitSnapshot(
      snapshot,
      [...new Set(w0)],
      state.owner,
      null,
      signal,
      deadline,
    );
  }

  async publishRemoval(
    claim: HubHostClaim,
    w0: readonly HubLiveDocument[],
    signal: AbortSignal,
    timeoutMs = DEFAULT_PUBLICATION_TIMEOUT_MS,
  ): Promise<HubSnapshot> {
    const deadline = this.#preparePublication(signal, timeoutMs);
    if (this.#claims.get(claim.featureId) !== claim) {
      throw new HubRegistryError("hub host claim revoked");
    }
    const active = this.#activeOwners.get(claim.featureId);
    if (active?.hostEpoch === claim.hostEpoch) {
      this.#activeOwners.delete(claim.featureId);
      this.#rejectOwnerWaiters(active);
    }
    for (const [key, entry] of this.#published) {
      if (entry.featureId === claim.featureId) {
        this.#published.delete(key);
      }
    }
    this.#revision += 1;
    const snapshot = this.#snapshot();
    this.#broadcastSnapshot(snapshot);
    return await this.#awaitSnapshot(
      snapshot,
      [...new Set(w0)],
      null,
      claim,
      signal,
      deadline,
      claim.featureId,
    );
  }

  async waitForCurrentSnapshot(
    owner: HubOwner,
    documents: readonly HubLiveDocument[],
    signal: AbortSignal,
    timeoutMs = DEFAULT_PUBLICATION_TIMEOUT_MS,
  ): Promise<HubSnapshot> {
    const deadline = this.#preparePublication(signal, timeoutMs);
    const claim = this.#claims.get(owner.featureId);
    if (
      !claim || claim.hostEpoch !== owner.hostEpoch ||
      !this.#isOwnerCurrent(owner)
    ) throw new HubRegistryError("hub generation is no longer current");
    return await this.#awaitSnapshot(
      this.#snapshot(),
      [...new Set(documents)],
      owner,
      claim,
      signal,
      deadline,
    );
  }

  async #awaitSnapshot(
    snapshot: HubSnapshot,
    documents: readonly HubLiveDocument[],
    owner: HubOwner | null,
    claim: HubHostClaim | null,
    signal: AbortSignal,
    deadline: number,
    absentFeatureId?: string,
  ): Promise<HubSnapshot> {
    this.#assertWaitCurrent(owner, claim, absentFeatureId);
    if (signal.aborted) throw this.#abortReason(signal);
    if (documents.length === 0) return snapshot;
    const waits = documents.map((document) =>
      this.#beginAckWait(
        document,
        owner,
        claim,
        snapshot,
        absentFeatureId,
      )
    );
    let timer = 0;
    let timerActive = false;
    let abortHandler: (() => void) | null = null;
    const timeout = new Promise<never>((_, reject) => {
      this.#pendingPublicationTimers += 1;
      timerActive = true;
      timer = setTimeout(
        () =>
          reject(new HubRegistryError("publication acknowledgement timed out")),
        Math.max(0, deadline - Date.now()),
      );
    });
    const aborted = new Promise<never>((_, reject) => {
      abortHandler = () => reject(this.#abortReason(signal));
      signal.addEventListener("abort", abortHandler, { once: true });
      if (signal.aborted) abortHandler();
    });
    try {
      await Promise.race([
        Promise.all(waits.map((wait) => wait.promise)),
        timeout,
        aborted,
      ]);
      if (signal.aborted) throw this.#abortReason(signal);
      this.#assertWaitCurrent(owner, claim, absentFeatureId);
      return snapshot;
    } finally {
      if (timerActive) {
        clearTimeout(timer);
        this.#pendingPublicationTimers -= 1;
      }
      if (abortHandler) signal.removeEventListener("abort", abortHandler);
      for (const wait of waits) wait.dispose();
    }
  }

  #registerDescriptor(lease: HubGenerationLease, json: string): void {
    const state = this.#leases.get(lease);
    if (!state || state.disposed || state.sealed) {
      throw new HubRegistryError(
        "hub generation is not accepting registrations",
      );
    }
    const descriptor = parseHubPageDescriptor(json, state.owner.featureId);
    if (this.#reservedRoutes.has(descriptor.route)) {
      throw new HubRegistryError("reserved hub route");
    }
    if (state.entries.has(descriptor.localId)) {
      throw new HubRegistryError("duplicate hub page ID");
    }
    const entry: HubSnapshotEntry = {
      ...descriptor,
      featureId: state.owner.featureId,
      ownerHostEpoch: state.owner.hostEpoch,
      ownerGeneration: state.owner.generation,
    };
    for (const candidate of this.#published.values()) {
      if (
        candidate.featureId !== entry.featureId &&
        candidate.route === entry.route
      ) {
        throw new HubRegistryError("duplicate hub route");
      }
    }
    for (const [otherLease, other] of this.#leases) {
      if (otherLease === lease || other.disposed) continue;
      for (const candidate of other.entries.values()) {
        if (
          candidate.featureId !== entry.featureId &&
          candidate.route === entry.route
        ) {
          throw new HubRegistryError("duplicate hub route");
        }
      }
    }
    if (state.entries.size >= MAX_ENTRIES_PER_OWNER) {
      throw new HubRegistryError("hub owner entry limit exceeded");
    }
    state.entries.set(descriptor.localId, entry);
  }

  #disposeGeneration(lease: HubGenerationLease): void {
    const state = this.#leases.get(lease);
    if (!state || state.disposed) return;
    state.disposed = true;
    this.#leases.delete(lease);
    const active = this.#activeOwners.get(state.owner.featureId);
    if (!active || !sameOwner(active, state.owner)) {
      this.#rejectOwnerWaiters(state.owner);
      return;
    }
    this.#activeOwners.delete(state.owner.featureId);
    let changed = false;
    for (const [key, entry] of this.#published) {
      if (
        entry.featureId === state.owner.featureId &&
        entry.ownerHostEpoch === state.owner.hostEpoch &&
        entry.ownerGeneration === state.owner.generation
      ) {
        this.#published.delete(key);
        changed = true;
      }
    }
    this.#rejectOwnerWaiters(state.owner);
    if (changed) {
      this.#revision += 1;
      this.#broadcastCurrentSnapshot();
    }
  }

  #releaseHost(claim: HubHostClaim): void {
    if (this.#claims.get(claim.featureId) !== claim) return;
    this.#rejectClaimWaiters(claim);
    for (const [lease, state] of [...this.#leases]) {
      if (
        state.owner.featureId === claim.featureId &&
        state.owner.hostEpoch === claim.hostEpoch
      ) {
        this.#disposeGeneration(lease);
      }
    }
    this.#claims.delete(claim.featureId);
    const global = hostClaims.get(claim.featureId);
    if (global?.registry === this && global.claim === claim) {
      hostClaims.delete(claim.featureId);
    }
  }

  #beginAckWait(
    document: HubLiveDocument,
    owner: HubOwner | null,
    claim: HubHostClaim | null,
    snapshot: HubSnapshot,
    absentFeatureId?: string,
  ): { promise: Promise<void>; dispose(): void } {
    let resolve!: () => void;
    let reject!: (error: Error) => void;
    const promise = new Promise<void>((res, rej) => {
      resolve = res;
      reject = rej;
    });
    const waiter: AckWaiter = {
      document,
      connection: null,
      epoch: snapshot.epoch,
      revision: snapshot.hubRevision,
      settled: false,
      owner,
      claim,
      absentFeatureId,
      resolve: () => {
        if (waiter.settled) return;
        waiter.settled = true;
        resolve();
      },
      reject: (error) => {
        if (waiter.settled) return;
        waiter.settled = true;
        reject(error);
      },
    };
    this.#waiters.add(waiter);
    let unsubscribeAvailable: (() => void) | null = null;
    let unsubscribeClosed: (() => void) | null = null;
    try {
      unsubscribeAvailable = document.onParentClientAvailable(() =>
        this.#connectWaiter(waiter)
      );
      unsubscribeClosed = document.onClosed(() => {
        if (!document.isLive()) {
          this.#removeClosedDocumentClient(waiter);
          waiter.resolve();
        }
      });
      this.#connectWaiter(waiter);
      if (!document.isLive()) waiter.resolve();
    } catch (error) {
      waiter.reject(error instanceof Error ? error : new Error(String(error)));
    }
    void promise.catch(() => {});
    return {
      promise,
      dispose: () => {
        try {
          unsubscribeAvailable?.();
        } catch (error) {
          console.error("[HubRegistry]", error);
        }
        try {
          unsubscribeClosed?.();
        } catch (error) {
          console.error("[HubRegistry]", error);
        }
        this.#waiters.delete(waiter);
      },
    };
  }

  #connectWaiter(waiter: AckWaiter): void {
    if (waiter.settled) return;
    if (!waiter.document.isLive()) {
      this.#removeClosedDocumentClient(waiter);
      waiter.resolve();
      return;
    }
    const connection = waiter.document.ensureParentClient();
    if (!connection || connection.isClosed()) return;
    if (connection.windowId !== waiter.document.windowId) {
      waiter.reject(
        new HubRegistryError("HUB client window identity mismatch"),
      );
      return;
    }
    const collision = [...this.#waiters].find((candidate) =>
      candidate !== waiter &&
      candidate.document.documentId !== waiter.document.documentId &&
      candidate.connection?.connectionId === connection.connectionId
    );
    if (collision) {
      waiter.reject(
        new HubRegistryError("HUB connection belongs to multiple documents"),
      );
      return;
    }
    if (waiter.connection && waiter.connection !== connection) {
      this.removeClient(waiter.connection);
    }
    waiter.connection = connection;
    try {
      this.registerClient(connection);
      this.#resolveWaiterFromRenderedAcknowledgement(waiter);
    } catch (error) {
      waiter.connection = null;
      waiter.reject(error instanceof Error ? error : new Error(String(error)));
    }
  }

  #removeClosedDocumentClient(waiter: AckWaiter): void {
    try {
      const connection = waiter.connection ??
        waiter.document.ensureParentClient();
      if (connection) this.removeClient(connection);
    } catch (error) {
      console.error("[HubRegistry]", error);
    }
  }

  #rejectOwnerWaiters(owner: HubOwner): void {
    for (const waiter of [...this.#waiters]) {
      if (waiter.owner && sameOwner(waiter.owner, owner)) {
        waiter.reject(new HubRegistryError("publication superseded"));
      }
    }
  }

  #rejectClaimWaiters(claim: HubHostClaim): void {
    for (const waiter of [...this.#waiters]) {
      if (waiter.claim === claim) {
        waiter.reject(new HubRegistryError("hub host claim revoked"));
      }
    }
  }

  #rejectRemovalWaiters(featureId: string): void {
    for (const waiter of [...this.#waiters]) {
      if (waiter.absentFeatureId === featureId) {
        waiter.reject(new HubRegistryError("HUB removal superseded"));
      }
    }
  }

  #isOwnerCurrent(owner: HubOwner): boolean {
    const current = this.#activeOwners.get(owner.featureId);
    return current !== undefined && sameOwner(current, owner);
  }

  #isWaitCurrent(waiter: AckWaiter): boolean {
    return this.#isWaitOwnerClaimCurrent(
      waiter.owner,
      waiter.claim,
      waiter.absentFeatureId,
    );
  }

  #isWaitOwnerClaimCurrent(
    owner: HubOwner | null,
    claim: HubHostClaim | null,
    absentFeatureId?: string,
  ): boolean {
    if (owner && !this.#isOwnerCurrent(owner)) return false;
    if (claim && this.#claims.get(claim.featureId) !== claim) return false;
    if (
      absentFeatureId &&
      [...this.#published.values()].some((entry) =>
        entry.featureId === absentFeatureId
      )
    ) return false;
    return owner !== null || claim !== null;
  }

  #resolveWaiterFromRenderedAcknowledgement(waiter: AckWaiter): void {
    if (!waiter.connection || !this.#isWaitCurrent(waiter)) return;
    const rendered = this.#renderedAcknowledgement.get(waiter.connection);
    if (
      rendered?.epoch === waiter.epoch && rendered.revision >= waiter.revision
    ) waiter.resolve();
  }

  #assertWaitCurrent(
    owner: HubOwner | null,
    claim: HubHostClaim | null,
    absentFeatureId?: string,
  ): void {
    if (!this.#isWaitOwnerClaimCurrent(owner, claim, absentFeatureId)) {
      throw new HubRegistryError("publication superseded");
    }
  }

  #preparePublication(signal: AbortSignal, timeoutMs: number): number {
    if (signal.aborted) throw this.#abortReason(signal);
    if (
      !Number.isSafeInteger(timeoutMs) || timeoutMs < 1 || timeoutMs > 10_000
    ) throw new HubRegistryError("invalid HUB publication deadline");
    return Date.now() + timeoutMs;
  }

  #abortReason(signal: AbortSignal): Error {
    const reason = signal.reason;
    if (reason instanceof Error) return reason;
    if (
      typeof reason === "object" && reason !== null && "message" in reason
    ) return new Error(String(reason.message));
    return new HubRegistryError(
      typeof reason === "string" && reason ? reason : "publication aborted",
    );
  }

  #assertUniqueRoutes(entries: ReadonlyMap<string, HubSnapshotEntry>): void {
    const routes = new Set(this.#reservedRoutes);
    for (const entry of entries.values()) {
      if (routes.has(entry.route)) {
        throw new HubRegistryError("duplicate hub route");
      }
      routes.add(entry.route);
    }
  }

  #snapshot(): HubSnapshot {
    const entries = [...this.#published.values()].sort((left, right) =>
      left.featureId.localeCompare(right.featureId) ||
      left.localId.localeCompare(right.localId)
    );
    return {
      epoch: this.#epoch,
      hubRevision: this.#revision,
      entries: JSON.parse(JSON.stringify(entries)) as HubSnapshotEntry[],
    };
  }

  #broadcastCurrentSnapshot(): void {
    const snapshot = this.#snapshot();
    const json = JSON.stringify({ type: "snapshot", snapshot });
    for (const connection of this.#clients.values()) {
      if (connection.isClosed()) {
        this.removeClient(connection);
        continue;
      }
      try {
        this.#sendSnapshot(connection, snapshot, json);
      } catch {
        if (connection.isClosed()) this.removeClient(connection);
      }
    }
  }

  #sendSnapshot(
    connection: HubClientConnection,
    snapshot: HubSnapshot,
    json = JSON.stringify({ type: "snapshot", snapshot }),
  ): void {
    this.#latestDelivered.set(connection, {
      epoch: snapshot.epoch,
      revision: snapshot.hubRevision,
    });
    try {
      connection.sendSnapshot(json);
    } catch (error) {
      this.#latestDelivered.delete(connection);
      this.#renderedAcknowledgement.delete(connection);
      throw error;
    }
  }

  #broadcastSnapshot(
    snapshot: HubSnapshot,
    json = JSON.stringify({
      type: "snapshot",
      snapshot,
    }),
  ): void {
    for (const connection of this.#clients.values()) {
      if (connection.isClosed()) {
        this.removeClient(connection);
        continue;
      }
      const delivered = this.#latestDelivered.get(connection);
      if (
        delivered?.epoch === snapshot.epoch &&
        delivered.revision === snapshot.hubRevision
      ) continue;
      try {
        this.#sendSnapshot(connection, snapshot, json);
      } catch (error) {
        if (connection.isClosed()) this.removeClient(connection);
        else console.error("[HubRegistry]", error);
      }
    }
  }
}

const sharedRegistryKey = Symbol.for("floorp.featureHub.sharedRegistry");
type RegistryGlobal = typeof globalThis & { [key: symbol]: unknown };
const registryGlobal = globalThis as RegistryGlobal;

export function getSharedHubRegistry(): HubRegistry {
  let registry = registryGlobal[sharedRegistryKey] as HubRegistry | undefined;
  if (!registry) {
    registry = new HubRegistry();
    Object.defineProperty(registryGlobal, sharedRegistryKey, {
      value: registry,
    });
  }
  return registry;
}

export function configureSharedHubClientDiscovery(
  adapter: HubClientDiscoveryAdapter,
  reservedRoutes: readonly string[] = [],
): HubRegistry {
  const registry = getSharedHubRegistry();
  registry.setDiscoveryAdapter(adapter);
  registry.setReservedRoutes(reservedRoutes);
  return registry;
}
