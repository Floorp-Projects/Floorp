// SPDX-License-Identifier: MPL-2.0

import {
  isWorkspaceSyncId,
  liveWorkspaceSyncTabs,
  mergeWorkspaceSyncDocuments,
  parseWorkspaceSyncDocument,
  updateWorkspaceSyncDocument,
  WORKSPACE_SYNC_CHANGED,
  WORKSPACE_SYNC_COLLECTION,
} from "../../common/workspace-sync.ts";
import {
  parseWorkspaceSyncRecord,
  workspaceMetadataRecord,
  workspaceTabRecord,
} from "../../common/workspace-sync-record.ts";
import type { WorkspaceSyncRecordValue } from "../../common/workspace-sync-record-types.ts";
import type {
  WorkspaceSyncDocument,
  WorkspaceSyncProvider,
  WorkspaceSyncSnapshot,
  WorkspaceSyncTab,
} from "../../common/workspace-sync-types.ts";

const { SyncEngine, Store, LegacyTracker } = ChromeUtils.importESModule(
  "resource://services-sync/engines.sys.mjs",
);
const { CryptoWrapper } = ChromeUtils.importESModule(
  "resource://services-sync/record.sys.mjs",
);
const { Utils } = ChromeUtils.importESModule(
  "resource://services-sync/util.sys.mjs",
);
const { JSONFile } = ChromeUtils.importESModule(
  "resource://gre/modules/JSONFile.sys.mjs",
);
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
);

interface SyncState {
  version: 1;
  accountUid: string | null;
  records: Record<string, WorkspaceSyncDocument>;
  metadataKnown: string[];
  selected: string[];
  excluded: string[];
  deleted: Record<string, number>;
  retired: string[];
  pendingCloses: Record<string, WorkspaceSyncTab[]>;
  incomingFailures: string[];
  pendingApplies: string[];
  pendingBaselines: Record<string, WorkspaceSyncSnapshot>;
}

function emptyState(accountUid: string | null = null): SyncState {
  return {
    version: 1,
    accountUid,
    records: {},
    metadataKnown: [],
    selected: [],
    excluded: [],
    deleted: {},
    retired: [],
    pendingCloses: {},
    incomingFailures: [],
    pendingApplies: [],
    pendingBaselines: {},
  };
}
const providers = new Set<WorkspaceSyncProvider>();
const retired = new Set<string>();
const captureErrors = new Map<string, string>();
const file = new JSONFile({
  path: PathUtils.join(PathUtils.profileDir, "floorp-workspaces-sync.json"),
  dataPostProcessor: (value: unknown): SyncState => {
    const raw = value as Partial<SyncState> | null;
    if (raw?.version !== undefined && raw.version !== 1) {
      throw new Error("Unsupported workspace sync storage version");
    }
    const result = emptyState(
      typeof raw?.accountUid === "string" ? raw.accountUid : null,
    );
    for (const [id, document] of Object.entries(raw?.records ?? {})) {
      if (!isWorkspaceSyncId(id) || document.workspaceId !== id) {
        throw new Error("Stored workspace ID mismatch");
      }
      // Validate each independently sized wire entity, rather than limiting the
      // aggregate profile cache or retaining closed-tab payloads indefinitely.
      parseWorkspaceSyncRecord(workspaceMetadataRecord(document));
      for (const tab of document.tabs) {
        if (!tab.deleted) {
          parseWorkspaceSyncRecord(workspaceTabRecord(id, tab));
        }
      }
      result.records[id] = parseWorkspaceSyncDocument(document);
    }
    for (
      const key of [
        "selected",
        "excluded",
        "metadataKnown",
        "retired",
        "incomingFailures",
        "pendingApplies",
      ] as const
    ) result[key] = (raw?.[key] ?? []).filter(isWorkspaceSyncId);
    for (const [id, timestamp] of Object.entries(raw?.deleted ?? {})) {
      if (
        isWorkspaceSyncId(id) && typeof timestamp === "number" &&
        Number.isFinite(timestamp)
      ) result.deleted[id] = timestamp;
    }
    for (const [id, tabs] of Object.entries(raw?.pendingCloses ?? {})) {
      if (isWorkspaceSyncId(id) && Array.isArray(tabs)) {
        const pending = parseWorkspaceSyncDocument({
          version: 1,
          workspaceId: id,
          name: "",
          icon: null,
          metadata: { modifiedAt: 0, modifiedBy: id },
          tabs,
        });
        if (pending.tabs.some((tab) => !tab.deleted)) {
          throw new Error("Invalid pending tab closure");
        }
        result.pendingCloses[id] = pending.tabs;
      }
    }
    for (const [id, snapshot] of Object.entries(raw?.pendingBaselines ?? {})) {
      if (!isWorkspaceSyncId(id) || snapshot.workspaceId !== id) {
        throw new Error("Invalid pending workspace baseline");
      }
      const validated = updateWorkspaceSyncDocument(null, snapshot, id, 0);
      result.pendingBaselines[id] = documentSnapshot(validated);
    }
    return result;
  },
});
let loaded: Promise<void> | undefined;
let initialized = false;
let trackedChanges = Promise.resolve();
const state = (): SyncState => file.data as SyncState;
const ready = (): Promise<void> =>
  loaded ??= file.load().then(() => {
    for (const id of state().retired) retired.add(id);
    initialized = true;
  });
// Invalidate synchronously: Firefox startOver does not wait for an active sync.
let accountEpoch = 0;
let accountAbort = new AbortController();
function assertAccountEpoch(expected: number): void {
  if (expected !== accountEpoch) {
    throw new Error("Workspace Sync account session was invalidated");
  }
}
let mutationTail = Promise.resolve();
function mutate<T>(operation: () => T | Promise<T>): Promise<T> {
  const result = mutationTail.then(operation);
  mutationTail = result.then(() => {}, () => {});
  return result;
}
function changed(id: string, local = false): void {
  file.saveSoon();
  Services.obs.notifyObservers(
    null as unknown as nsISupports,
    WORKSPACE_SYNC_CHANGED,
    id,
  );
  if (local) {
    Services.obs.notifyObservers(
      null as unknown as nsISupports,
      "floorp.workspaces.sync.local-change",
      id,
    );
  }
}
function retire(id: string): void {
  if (!retired.has(id)) {
    retired.add(id);
    state().retired.push(id);
  }
}
function tombstone(id: string): void {
  state().deleted[id] ??= Date.now() / 1000;
  changed(id, true);
}
function findTab(
  id: string,
): { document: WorkspaceSyncDocument; tab: WorkspaceSyncTab } | null {
  for (const document of Object.values(state().records)) {
    const tab = document.tabs.find((value) => value.id === id);
    if (tab) return { document, tab };
  }
  return null;
}
function deviceId(): string {
  const pref = "floorp.workspaces.sync.device-id";
  const previous = Services.prefs.getStringPref(pref, "");
  if (isWorkspaceSyncId(previous)) return previous;
  const value = Services.uuid.generateUUID().toString().replace(/[{}]/g, "");
  Services.prefs.setStringPref(pref, value);
  return value;
}

interface RegistrationWaiter {
  resolve(): void;
  reject(error: Error): void;
}
const registrationWaiters = new Set<RegistrationWaiter>();
function providersReady(): boolean {
  if (!Services.prefs.getBoolPref("floorp.workspaces.enabled", true)) {
    return true;
  }
  const registered = new Set([...providers].map((provider) => provider.window));
  for (const target of Services.wm.getEnumerator("navigator:browser")) {
    const browserWindow = target as unknown as Window;
    if (
      !browserWindow.closed &&
      !PrivateBrowsingUtils.isWindowPrivate(browserWindow) &&
      !registered.has(browserWindow)
    ) return false;
  }
  return true;
}
function registrationChanged(): void {
  if (!providersReady()) return;
  for (const waiter of [...registrationWaiters]) waiter.resolve();
}
function waitForProviders(signal?: AbortSignal): Promise<void> {
  if (signal?.aborted) {
    return Promise.reject(new Error("Workspace window registration cancelled"));
  }
  if (providersReady()) return Promise.resolve();
  return new Promise((resolve, reject) => {
    const finish = (error?: Error) => {
      registrationWaiters.delete(waiter);
      clearTimeout(timer);
      signal?.removeEventListener("abort", abort);
      if (error) reject(error);
      else resolve();
    };
    const waiter = {
      resolve: () => finish(),
      reject: (error: Error) => finish(error),
    };
    const abort = () =>
      finish(new Error("Workspace window registration cancelled"));
    const timer = setTimeout(
      () =>
        finish(
          new Error(
            "Workspace windows did not finish initializing within 30 seconds",
          ),
        ),
      30_000,
    );
    registrationWaiters.add(waiter);
    signal?.addEventListener("abort", abort, { once: true });
  });
}
Services.obs.addObserver(registrationChanged, "domwindowclosed");
Services.prefs.addObserver("floorp.workspaces.enabled", registrationChanged);
Services.obs.addObserver(() => {
  for (const waiter of [...registrationWaiters]) {
    waiter.reject(new Error("Browser is shutting down"));
  }
}, "quit-application-granted");

async function apply(
  document: WorkspaceSyncDocument,
  expected = accountEpoch,
): Promise<void> {
  assertAccountEpoch(expected);
  const signal = accountAbort.signal;
  const targets = [...providers].filter((provider) => !provider.window.closed);
  const known = new Set(
    targets.flatMap((provider) => provider.tabIds(document.workspaceId)),
  );
  const missing = new Set(
    liveWorkspaceSyncTabs(document).map((tab) => tab.id).filter((id) =>
      !known.has(id)
    ),
  );
  for (let index = 0; index < targets.length; index++) {
    if (!targets[index].window.closed) {
      assertAccountEpoch(expected);
      await targets[index].apply(
        document,
        index === 0 ? missing : new Set(),
        signal,
      );
      assertAccountEpoch(expected);
    }
  }
}
function captureSnapshot(
  id: string,
  seenIds = new Set<string>(Object.keys(state().records)),
): WorkspaceSyncSnapshot | null {
  const snapshots = [...providers].filter((provider) => !provider.window.closed)
    .map((provider) => provider.capture(id, seenIds))
    .filter((value): value is WorkspaceSyncSnapshot => value !== null);
  if (!snapshots.length) return null;
  return {
    ...snapshots[0],
    orderChanged: snapshots.some((snapshot) => snapshot.orderChanged !== false),
    tabs: snapshots.flatMap((value) => value.tabs),
  };
}
function documentSnapshot(
  document: WorkspaceSyncDocument,
): WorkspaceSyncSnapshot {
  return {
    workspaceId: document.workspaceId,
    name: document.name,
    icon: document.icon,
    orderChanged: false,
    tabs: liveWorkspaceSyncTabs(document).map(({ id, url, title, pinned }) => ({
      id,
      url,
      title,
      pinned,
    })),
  };
}
function pendingSnapshot(
  desired: WorkspaceSyncDocument,
  observed: WorkspaceSyncSnapshot,
): WorkspaceSyncSnapshot {
  const snapshot = documentSnapshot(desired);
  const baseline = state().pendingBaselines[desired.workspaceId];
  if (!baseline) return snapshot;
  if (observed.name !== baseline.name) snapshot.name = observed.name;
  if (observed.icon !== baseline.icon) snapshot.icon = observed.icon;
  const before = new Map(baseline.tabs.map((tab) => [tab.id, tab]));
  const current = new Map(observed.tabs.map((tab) => [tab.id, tab]));
  const ids = new Set(snapshot.tabs.map((tab) => tab.id));
  snapshot.tabs = snapshot.tabs.filter((tab) => {
    // A tab observed before the failed apply can still be explicitly closed by
    // the user. A missing remote addition was never observed, so retain it.
    return !before.has(tab.id) || current.has(tab.id);
  }).map((tab) => {
    const previous = before.get(tab.id);
    const local = current.get(tab.id);
    // An unchanged physical tab may still contain the old value because the UI
    // apply failed. Only actual edits since the baseline supersede desired data.
    if (!local) return tab;
    if (!previous) return local;
    return {
      ...tab,
      url: local.url !== previous.url ? local.url : tab.url,
      title: local.title !== previous.title ? local.title : tab.title,
      pinned: local.pinned !== previous.pinned ? local.pinned : tab.pinned,
    };
  });
  const localAddition = observed.tabs.some((tab) =>
    !ids.has(tab.id) && !retired.has(tab.id)
  );
  for (const tab of observed.tabs) {
    if (!ids.has(tab.id) && !retired.has(tab.id)) snapshot.tabs.push(tab);
  }
  const sharedIds = new Set(
    baseline.tabs.map((tab) => tab.id).filter((id) =>
      current.has(id) && ids.has(id)
    ),
  );
  const beforeOrder = baseline.tabs.filter((tab) => sharedIds.has(tab.id))
    .map((tab) => tab.id);
  const currentOrder = observed.tabs.filter((tab) => sharedIds.has(tab.id))
    .map((tab) => tab.id);
  if (
    observed.orderChanged !== false && (
      localAddition ||
      beforeOrder.some((id, index) => id !== currentOrder[index])
    )
  ) {
    const desiredTabs = new Map(snapshot.tabs.map((tab) => [tab.id, tab]));
    const localOrder = observed.tabs.map((tab) => desiredTabs.get(tab.id))
      .filter((tab): tab is WorkspaceSyncSnapshot["tabs"][number] =>
        Boolean(tab)
      );
    const localIds = new Set(localOrder.map((tab) => tab.id));
    snapshot.tabs = [
      ...localOrder,
      ...snapshot.tabs.filter((tab) => !localIds.has(tab.id)),
    ];
    snapshot.orderChanged = true;
  }
  // Remote additions absent from the baseline remain desired until the failed
  // apply completes, rather than being mistaken for local closures.
  return snapshot;
}
function captureNow(): void {
  if (!Services.prefs.getBoolPref("floorp.workspaces.enabled", true)) return;
  const seenIds = new Set<string>(Object.keys(state().records));
  for (const id of state().selected) {
    try {
      const observed = captureSnapshot(id, seenIds);
      if (!observed) continue;
      const pending = state().pendingApplies.includes(id);
      const snapshot = pending && state().records[id]
        ? pendingSnapshot(state().records[id], observed)
        : observed;
      const previous = state().records[id] ?? null;
      const next = updateWorkspaceSyncDocument(
        previous,
        snapshot,
        deviceId(),
        Date.now(),
      );
      // Validate all outgoing entities before committing any part of a capture.
      const metadata = parseWorkspaceSyncRecord(workspaceMetadataRecord(next));
      for (const tab of next.tabs) {
        if (!tab.deleted) parseWorkspaceSyncRecord(workspaceTabRecord(id, tab));
      }
      state().records[id] = next;
      if (pending) state().pendingBaselines[id] = observed;
      if (!state().metadataKnown.includes(id)) state().metadataKnown.push(id);
      if (
        !previous ||
        JSON.stringify(workspaceMetadataRecord(previous)) !==
          JSON.stringify(metadata)
      ) changed(id, true);
      const oldTabs = new Map(previous?.tabs.map((tab) => [tab.id, tab]));
      for (const tab of next.tabs) {
        if (JSON.stringify(oldTabs.get(tab.id)) === JSON.stringify(tab)) {
          continue;
        }
        if (tab.deleted) {
          retire(tab.id);
          tombstone(tab.id);
        } else changed(tab.id, true);
      }

      if (captureErrors.delete(id)) {
        Services.prefs.setStringPref(
          "services.sync.floorp-workspaces.lastSync",
          "0",
        );
        changed(id);
      }
    } catch (error) {
      console.error("[WorkspacesSync] Could not capture workspace", id, error);
      captureErrors.set(id, "invalid-workspace");
      changed(id);
    }
  }
}
async function applySelected(
  document: WorkspaceSyncDocument,
  expected = accountEpoch,
  completesClosures = false,
): Promise<void> {
  assertAccountEpoch(expected);
  const id = document.workspaceId;
  if (!state().pendingApplies.includes(id)) state().pendingApplies.push(id);
  const baseline = captureSnapshot(id);
  if (baseline) state().pendingBaselines[id] = baseline;
  changed(id);
  await apply(document, expected);
  assertAccountEpoch(expected);
  if (completesClosures || !state().pendingCloses[id]?.length) {
    state().pendingApplies = state().pendingApplies.filter((value) =>
      value !== id
    );
    delete state().pendingBaselines[id];
  } else {
    const current = captureSnapshot(id);
    if (current) state().pendingBaselines[id] = current;
  }
  changed(id);
}
async function retryPendingApplies(expected: number): Promise<void> {
  for (const id of [...state().pendingApplies]) {
    assertAccountEpoch(expected);
    const document = state().records[id];
    if (document && state().selected.includes(id) && !retired.has(id)) {
      // Only a completed incoming pass can decide whether a later parent
      // tombstone cancels these closures. Ordinary capture must not flush them.
      if (state().pendingCloses[id]?.length) continue;
      await applySelected(document, expected);
    } else {
      state().pendingApplies = state().pendingApplies.filter((value) =>
        value !== id
      );
      delete state().pendingBaselines[id];
      file.saveSoon();
    }
  }
}
async function disarmAccount(): Promise<void> {
  ++accountEpoch;
  accountAbort.abort();
  accountAbort = new AbortController();
  await ready();
  await mutate(() => {
    file.data = emptyState();
    retired.clear();
    captureErrors.clear();
    sentTombstones.clear();
    file.saveSoon();
    Services.obs.notifyObservers(
      null as unknown as nsISupports,
      WORKSPACE_SYNC_CHANGED,
    );
  });
}
let accountReset = Promise.resolve();
Services.obs.addObserver(() => {
  accountReset = disarmAccount();
  accountReset.catch(console.error);
}, "weave:service:start-over");

/** Profile-scoped state; browsing history, cookies and containers never enter the transport. */
export const FloorpWorkspacesSync = {
  ready,
  isSelected(id: string): boolean {
    return initialized && state().selected.includes(id);
  },
  errorFor(id: string): string | null {
    if (retired.has(id)) return "deleted-workspace";
    if (initialized && state().pendingApplies.includes(id)) {
      return "incoming-failed";
    }
    if (
      initialized && state().incomingFailures.length &&
      state().pendingCloses[id]?.length
    ) return "incoming-failed";
    return captureErrors.get(id) ?? null;
  },
  isRetired(id: string): boolean {
    return retired.has(id);
  },
  isPendingClose(id: string): boolean {
    return initialized &&
      Object.values(state().pendingCloses).some((tabs) =>
        tabs.some((tab) => tab.id === id)
      );
  },
  async register(
    provider: WorkspaceSyncProvider,
    signal?: AbortSignal,
  ): Promise<() => void> {
    const unregister = () => {
      providers.delete(provider);
      registrationChanged();
    };
    providers.add(provider);
    signal?.addEventListener("abort", unregister, { once: true });
    registrationChanged();
    try {
      await ready();
      await waitForProviders(signal);
      await mutate(async () => {
        const expected = accountEpoch;
        if (signal?.aborted) {
          throw new Error("Workspace window registration cancelled");
        }
        captureNow();
        for (const id of state().selected) {
          if (state().records[id]) {
            await applySelected(state().records[id], expected);
          }
        }
      });
      return () => {
        signal?.removeEventListener("abort", unregister);
        unregister();
      };
    } catch (error) {
      signal?.removeEventListener("abort", unregister);
      unregister();
      throw error;
    }
  },
  async select(id: string, selected: boolean): Promise<void> {
    const expected = accountEpoch;
    await ready();
    await waitForProviders();
    if (!isWorkspaceSyncId(id)) throw new Error("Invalid workspace ID");
    await mutate(() => {
      assertAccountEpoch(expected);
      if (selected) {
        const previousSelected = [...state().selected];
        const previousExcluded = [...state().excluded];
        if (retired.has(id)) {
          throw new Error(
            "This workspace was deleted from Sync. Restore or create a new workspace to sync it again.",
          );
        }
        state().excluded = state().excluded.filter((value) => value !== id);
        delete state().deleted[id];
        if (!state().selected.includes(id)) state().selected.push(id);
        captureNow();
        if (captureErrors.has(id)) {
          state().selected = previousSelected;
          state().excluded = previousExcluded;
          throw new Error("This workspace could not be prepared for sync");
        }
        // Opt-in must fetch records that changed while this workspace was excluded.
        Services.prefs.setStringPref(
          "services.sync.floorp-workspaces.lastSync",
          "0",
        );
      } else {
        state().selected = state().selected.filter((value) => value !== id);
        if (!state().excluded.includes(id)) state().excluded.push(id);
      }
      changed(id);
    });
  },
  async capture(): Promise<void> {
    const expected = accountEpoch;
    await ready();
    await waitForProviders(accountAbort.signal);
    await mutate(async () => {
      assertAccountEpoch(expected);
      captureNow();
      await retryPendingApplies(expected);
    });
  },
  async deleteWorkspace(id: string): Promise<void> {
    const expected = accountEpoch;
    await ready();
    await mutate(() => {
      assertAccountEpoch(expected);
      if (!state().selected.includes(id)) return;
      for (const tab of state().records[id]?.tabs ?? []) {
        retire(tab.id);
        tombstone(tab.id);
      }
      delete state().records[id];
      delete state().pendingCloses[id];
      delete state().pendingBaselines[id];
      state().pendingApplies = state().pendingApplies.filter((value) =>
        value !== id
      );
      state().metadataKnown = state().metadataKnown.filter((value) =>
        value !== id
      );
      state().selected = state().selected.filter((value) => value !== id);
      if (!state().excluded.includes(id)) state().excluded.push(id);
      retire(id);
      tombstone(id);
    });
  },
};

export class FloorpWorkspaceRecord extends CryptoWrapper {
  declare entity: WorkspaceSyncRecordValue;
  constructor(collection: string, id: string) {
    super(collection, id);
  }
}
Utils.deferGetSet(FloorpWorkspaceRecord, "cleartext", ["entity"]);
const sentTombstones = new Map<string, number>();
// Native decrypt/reconcile callbacks can resume after startOver. Preserve the
// session in which each incoming record entered the engine, even after rebind.
const recordEpochs = new WeakMap<FloorpWorkspaceRecord, number>();

class FloorpWorkspaceStore extends Store {
  async getAllIDs(): Promise<Record<string, boolean>> {
    const expected = this.engine.accountSessionEpoch;
    await ready();
    this.engine.assertAccountSession(expected);
    const ids = [...Object.keys(state().deleted)];
    for (const id of state().selected) {
      if (!state().records[id] || captureErrors.has(id)) continue;
      ids.push(
        id,
        ...state().records[id].tabs.filter((tab) => !tab.deleted).map((tab) =>
          tab.id
        ),
      );
    }
    return Object.fromEntries(ids.map((id) => [id, true]));
  }
  async itemExists(id: string): Promise<boolean> {
    const expected = this.engine.accountSessionEpoch;
    await ready();
    this.engine.assertAccountSession(expected);
    if (state().records[id]) return state().selected.includes(id);
    const entry = findTab(id);
    return Boolean(
      entry && !entry.tab.deleted &&
        state().selected.includes(entry.document.workspaceId),
    );
  }
  async createRecord(
    id: string,
    collection: string,
  ): Promise<FloorpWorkspaceRecord> {
    const expected = this.engine.accountSessionEpoch;
    await ready();
    this.engine.assertAccountSession(expected);
    const record = new FloorpWorkspaceRecord(collection, id);
    if (Object.hasOwn(state().deleted, id)) {
      record.deleted = true;
      sentTombstones.set(id, state().deleted[id]);
      return record;
    }
    const document = state().records[id];
    if (document && state().selected.includes(id)) {
      record.entity = workspaceMetadataRecord(document);
    } else {
      const entry = findTab(id);
      if (
        !entry || entry.tab.deleted ||
        !state().selected.includes(entry.document.workspaceId)
      ) throw new Error("Workspace is no longer selected for sync");
      record.entity = workspaceTabRecord(entry.document.workspaceId, entry.tab);
    }
    parseWorkspaceSyncRecord(record.entity);
    return record;
  }
  async create(record: FloorpWorkspaceRecord): Promise<void> {
    const expected = recordEpochs.get(record) ??
      this.engine.accountSessionEpoch;
    await ready();
    this.engine.assertAccountSession(expected);
    await waitForProviders(accountAbort.signal);
    await mutate(async () => {
      this.engine.assertAccountSession(expected);
      const entity = parseWorkspaceSyncRecord(record.entity);
      if (
        record.id !==
          (entity.kind === "workspace" ? entity.workspaceId : entity.tab.id)
      ) throw new Error("Incoming record ID mismatch");
      if (entity.kind === "workspace" && findTab(record.id)) {
        throw new Error("Workspace and tab IDs must be distinct");
      }
      if (entity.kind === "tab" && state().records[record.id]) {
        throw new Error("Workspace and tab IDs must be distinct");
      }
      // Capture edits made while the network was in flight before applying a
      // complete merged workspace. Pending applies/closures are excluded.
      captureNow();
      if (
        state().excluded.includes(entity.workspaceId) ||
        captureErrors.has(entity.workspaceId)
      ) return;
      if (
        Object.hasOwn(state().deleted, record.id) ||
        (retired.has(record.id) || retired.has(entity.workspaceId))
      ) {
        tombstone(record.id);
        this.engine._modified.set(record.id, Date.now() / 1000);
        return;
      }
      let document = state().records[entity.workspaceId];
      if (entity.kind === "workspace") {
        const incoming: WorkspaceSyncDocument = {
          version: 1,
          workspaceId: entity.workspaceId,
          name: entity.name,
          icon: entity.icon,
          metadata: entity.metadata,
          tabs: [],
        };
        document =
          document && state().metadataKnown.includes(entity.workspaceId)
            ? mergeWorkspaceSyncDocuments(document, incoming)
            : { ...incoming, tabs: document?.tabs ?? [] };
        if (!state().metadataKnown.includes(entity.workspaceId)) {
          state().metadataKnown.push(entity.workspaceId);
        }
        if (!state().selected.includes(entity.workspaceId)) {
          state().selected.push(entity.workspaceId);
        }
        if (
          JSON.stringify(workspaceMetadataRecord(document)) !==
            JSON.stringify(entity)
        ) this.engine._modified.set(record.id, Date.now() / 1000);
        else this.engine._modified.delete(record.id);
      } else {
        const existing = findTab(record.id);
        if (existing && existing.document.workspaceId !== entity.workspaceId) {
          throw new Error("A tab ID cannot move between workspaces");
        }
        document ??= {
          version: 1,
          workspaceId: entity.workspaceId,
          name: "",
          icon: null,
          metadata: { modifiedAt: 0, modifiedBy: entity.tab.modifiedBy },
          tabs: [],
        };
        document = mergeWorkspaceSyncDocuments(document, {
          ...document,
          tabs: [entity.tab],
        });
        const merged = document.tabs.find((tab) => tab.id === record.id);
        if (!merged) throw new Error("Incoming tab is missing after merge");
        if (
          JSON.stringify(workspaceTabRecord(entity.workspaceId, merged)) !==
            JSON.stringify(entity)
        ) this.engine._modified.set(record.id, Date.now() / 1000);
        else this.engine._modified.delete(record.id);
      }
      state().records[entity.workspaceId] = document;
      changed(entity.workspaceId);
      if (state().selected.includes(entity.workspaceId)) {
        await applySelected(document, expected);
        this.engine.assertAccountSession(expected);
      }
    });
  }
  async update(record: FloorpWorkspaceRecord): Promise<void> {
    await this.create(record);
  }
  async remove(record: FloorpWorkspaceRecord): Promise<void> {
    const expected = recordEpochs.get(record) ??
      this.engine.accountSessionEpoch;
    await ready();
    this.engine.assertAccountSession(expected);
    await mutate(() => {
      this.engine.assertAccountSession(expected);
      delete state().deleted[record.id];
      retire(record.id);
      if (state().records[record.id]) {
        // Workspace deletion disables sync while preserving this device's content.
        delete state().records[record.id];
        delete state().pendingCloses[record.id];
        delete state().pendingBaselines[record.id];
        state().pendingApplies = state().pendingApplies.filter((id) =>
          id !== record.id
        );
        state().metadataKnown = state().metadataKnown.filter((id) =>
          id !== record.id
        );
        state().selected = state().selected.filter((id) => id !== record.id);
        if (!state().excluded.includes(record.id)) {
          state().excluded.push(record.id);
        }
      } else {
        retire(record.id);
        const entry = findTab(record.id);
        if (entry) {
          const id = entry.document.workspaceId;
          if (!state().pendingCloses[id]) state().pendingCloses[id] = [];
          if (!state().pendingCloses[id].some((tab) => tab.id === record.id)) {
            state().pendingCloses[id].push({ ...entry.tab, deleted: true });
          }
          entry.document.tabs = entry.document.tabs.filter((tab) =>
            tab.id !== record.id
          );
        }
      }
      changed(record.id);
    });
  }
  changeItemID(): Promise<void> {
    return Promise.reject(new Error("Workspace Sync IDs are immutable"));
  }
  async wipe(): Promise<void> {
    const expected = this.engine.accountSessionEpoch;
    await ready();
    this.engine.assertAccountSession(expected);
    await mutate(() => {
      this.engine.assertAccountSession(expected);
      const next = emptyState(state().accountUid);
      next.excluded = [...new Set([...state().excluded, ...state().selected])];
      file.data = next;
      retired.clear();
      changed("");
    });
  }
}
class FloorpWorkspaceTracker extends LegacyTracker {
  onStart(): void {
    Services.obs.addObserver(this, "floorp.workspaces.sync.local-change");
  }
  onStop(): void {
    Services.obs.removeObserver(this, "floorp.workspaces.sync.local-change");
  }
  observe(_subject: unknown, _topic: string, id: string): void {
    // This topic only represents local edits, including edits during Sync.
    trackedChanges = trackedChanges.catch(() => {}).then(async () => {
      await this._saveChangedID(id, Date.now() / 1000);
      this.score += 10;
    });
    trackedChanges.catch(console.error);
  }
}

/** Firefox owns authenticated transport, encryption, collection state and retry. */
export class FloorpWorkspacesEngine extends SyncEngine {
  accountSessionEpoch = accountEpoch;
  private accountUid: string | undefined;
  constructor(service: unknown) {
    super(WORKSPACE_SYNC_COLLECTION, service);
  }
  assertAccountSession(expected = this.accountSessionEpoch): void {
    assertAccountEpoch(expected);
    if (expected !== this.accountSessionEpoch) {
      throw new Error("Workspace Sync engine session changed");
    }
    if (
      this.accountUid !== undefined && state().accountUid !== this.accountUid
    ) {
      throw new Error("Workspace Sync account identity changed");
    }
  }
  get enabled(): boolean {
    return super.enabled &&
      Services.prefs.getBoolPref("floorp.workspaces.enabled", true);
  }
  set enabled(value: boolean) {
    super.enabled = value;
  }
  async _processIncoming(): Promise<void> {
    const expected = this.accountSessionEpoch;
    this.assertAccountSession(expected);
    await super._processIncoming();
    this.assertAccountSession(expected);
    // The base engine can resolve with failed or backlogged records. A parent
    // tombstone may still be among them, so defer UI closure until all incoming
    // records have been applied, across every download batch.
    if (state().incomingFailures.length) {
      const backlog = this.toFetch;
      for (const id of state().incomingFailures) backlog.add(id);
      this.toFetch = backlog;
      return;
    }
    if (this.toFetch.size || this.previousFailedIn.size) return;
    await this._flushIncomingTabs();
  }
  async _maybeReconcile(
    record: FloorpWorkspaceRecord,
  ): Promise<{ shouldApply: boolean; error: unknown }> {
    const expected = recordEpochs.get(record) ?? this.accountSessionEpoch;
    recordEpochs.set(record, expected);
    this.assertAccountSession(expected);
    const result = await super._maybeReconcile(record);
    await ready();
    await mutate(() => {
      this.assertAccountSession(expected);
      if (result.error) {
        if (
          isWorkspaceSyncId(record.id) &&
          !state().incomingFailures.includes(record.id)
        ) state().incomingFailures.push(record.id);
      } else if (!result.shouldApply) {
        state().incomingFailures = state().incomingFailures.filter((id) =>
          id !== record.id
        );
      }
      file.saveSoon();
    });
    return result;
  }
  async _applyRecords(
    records: FloorpWorkspaceRecord[],
    telemetry: unknown,
  ): Promise<string[]> {
    const expected = this.accountSessionEpoch;
    this.assertAccountSession(expected);
    for (const record of records) {
      if (!recordEpochs.has(record)) recordEpochs.set(record, expected);
    }
    const failed = await super._applyRecords(records, telemetry) as string[];
    await ready();
    await mutate(() => {
      this.assertAccountSession(expected);
      const failures = new Set(state().incomingFailures);
      for (const record of records) {
        if (failed.includes(record.id)) failures.add(record.id);
        else failures.delete(record.id);
      }
      state().incomingFailures = [...failures];
      file.saveSoon();
    });
    return failed;
  }
  async _flushIncomingTabs(): Promise<void> {
    const expected = this.accountSessionEpoch;
    await ready();
    this.assertAccountSession(expected);
    await waitForProviders(accountAbort.signal);
    await mutate(async () => {
      this.assertAccountSession(expected);
      captureNow();
      for (const [id, tabs] of Object.entries(state().pendingCloses)) {
        const document = state().records[id];
        if (document && state().selected.includes(id) && !retired.has(id)) {
          await applySelected(
            { ...document, tabs: [...document.tabs, ...tabs] },
            expected,
            true,
          );
          this.assertAccountSession(expected);
        }
        delete state().pendingCloses[id];
      }
      file.saveSoon();
    });
  }
  async getChangedIDs(): Promise<Record<string, number>> {
    const expected = this.accountSessionEpoch;
    await ready();
    this.assertAccountSession(expected);
    const tracked = await this._tracker.getChangedIDs() as Record<
      string,
      number
    >;
    this.assertAccountSession(expected);
    return Object.fromEntries(
      Object.entries(tracked).filter(([id]) => {
        if (Object.hasOwn(state().deleted, id)) return true;
        const workspaceId = state().records[id]
          ? id
          : findTab(id)?.document.workspaceId;
        return workspaceId && state().selected.includes(workspaceId) &&
          !captureErrors.has(workspaceId);
      }),
    );
  }
  async _syncStartup(): Promise<void> {
    let expected = accountEpoch;
    await accountReset;
    assertAccountEpoch(expected);
    const { getFxAccountsSingleton } = ChromeUtils.importESModule(
      "resource://gre/modules/FxAccounts.sys.mjs",
    );
    const user = await getFxAccountsSingleton().getSignedInUser();
    if (!user?.uid) {
      throw new Error("Workspace Sync requires a signed-in Mozilla Account");
    }
    await ready();
    assertAccountEpoch(expected);
    if (state().accountUid && state().accountUid !== user.uid) {
      const reset = disarmAccount();
      expected = accountEpoch;
      accountReset = reset;
      await reset;
    }
    await mutate(() => {
      assertAccountEpoch(expected);
      this.accountSessionEpoch = expected;
      this.accountUid = user.uid;
      state().accountUid = user.uid;
      file.saveSoon();
    });
    await FloorpWorkspacesSync.capture();
    await trackedChanges;
    this.assertAccountSession(expected);
    await super._syncStartup();
    this.assertAccountSession(expected);
  }
  async _reconcile(record: FloorpWorkspaceRecord): Promise<boolean> {
    const expected = recordEpochs.get(record) ?? this.accountSessionEpoch;
    recordEpochs.set(record, expected);
    await ready();
    this.assertAccountSession(expected);
    if (!isWorkspaceSyncId(record.id)) {
      throw new Error("Invalid incoming Sync record ID");
    }
    if (record.deleted) {
      this._modified.delete(record.id);
      return true;
    }
    const entity = parseWorkspaceSyncRecord(record.entity);
    if (
      record.id !==
        (entity.kind === "workspace" ? entity.workspaceId : entity.tab.id)
    ) throw new Error("Incoming record ID mismatch");
    if (
      state().excluded.includes(entity.workspaceId) ||
      captureErrors.has(entity.workspaceId)
    ) {
      this._modified.delete(record.id);
      return false;
    }
    if (
      Object.hasOwn(state().deleted, record.id) ||
      (retired.has(record.id) || retired.has(entity.workspaceId))
    ) {
      await mutate(() => {
        this.assertAccountSession(expected);
        tombstone(record.id);
      });
      this.assertAccountSession(expected);
      this._modified.set(record.id, Date.now() / 1000);
      return false;
    }
    return true;
  }
  async _onRecordsWritten(succeeded: string[]): Promise<void> {
    const expected = this.accountSessionEpoch;
    await ready();
    await mutate(() => {
      this.assertAccountSession(expected);
      for (const id of succeeded) {
        if (
          !sentTombstones.has(id) ||
          sentTombstones.get(id) !== state().deleted[id]
        ) continue;
        delete state().deleted[id];
        sentTombstones.delete(id);
        const entry = findTab(id);
        if (entry?.tab.deleted) {
          entry.document.tabs = entry.document.tabs.filter((tab) =>
            tab.id !== id
          );
        }
      }
      file.saveSoon();
    });
  }
}
Object.assign(FloorpWorkspacesEngine.prototype, {
  _storeObj: FloorpWorkspaceStore,
  _trackerObj: FloorpWorkspaceTracker,
  _recordObj: FloorpWorkspaceRecord,
  version: 1,
  syncPriority: 4,
  allowSkippedRecord: false,
});
