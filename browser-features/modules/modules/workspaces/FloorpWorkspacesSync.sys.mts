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

async function apply(document: WorkspaceSyncDocument): Promise<void> {
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
      await targets[index].apply(document, index === 0 ? missing : new Set());
    }
  }
}
function captureNow(): void {
  if (!Services.prefs.getBoolPref("floorp.workspaces.enabled", true)) return;
  const seenIds = new Set<string>(Object.keys(state().records));
  for (const id of state().selected) {
    try {
      const snapshots = [...providers].filter((provider) =>
        !provider.window.closed
      )
        .map((provider) => provider.capture(id, seenIds))
        .filter((value): value is WorkspaceSyncSnapshot => value !== null);
      if (!snapshots.length) continue;
      const snapshot = {
        ...snapshots[0],
        orderChanged: snapshots.some((snapshot) =>
          snapshot.orderChanged !== false
        ),
        tabs: snapshots.flatMap((value) => value.tabs),
      };
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
async function disarmAccount(): Promise<void> {
  await ready();
  await mutate(() => {
    file.data = emptyState();
    retired.clear();
    captureErrors.clear();
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
        if (signal?.aborted) {
          throw new Error("Workspace window registration cancelled");
        }
        captureNow();
        for (const id of state().selected) {
          if (state().records[id]) await apply(state().records[id]);
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
    await ready();
    await waitForProviders();
    if (!isWorkspaceSyncId(id)) throw new Error("Invalid workspace ID");
    await mutate(() => {
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
    await ready();
    await waitForProviders();
    await mutate(captureNow);
  },
  async deleteWorkspace(id: string): Promise<void> {
    await ready();
    await mutate(() => {
      if (!state().selected.includes(id)) return;
      for (const tab of state().records[id]?.tabs ?? []) {
        retire(tab.id);
        tombstone(tab.id);
      }
      delete state().records[id];
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

class FloorpWorkspaceStore extends Store {
  async getAllIDs(): Promise<Record<string, boolean>> {
    await ready();
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
    await ready();
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
    await ready();
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
    await ready();
    await waitForProviders();
    await mutate(async () => {
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
      if (state().selected.includes(entity.workspaceId)) await apply(document);
    });
  }
  async update(record: FloorpWorkspaceRecord): Promise<void> {
    await this.create(record);
  }
  async remove(record: FloorpWorkspaceRecord): Promise<void> {
    await ready();
    await mutate(() => {
      delete state().deleted[record.id];
      retire(record.id);
      if (state().records[record.id]) {
        // Workspace deletion disables sync while preserving this device's content.
        delete state().records[record.id];
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
    await ready();
    await mutate(() => {
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
  constructor(service: unknown) {
    super(WORKSPACE_SYNC_COLLECTION, service);
  }
  get enabled(): boolean {
    return super.enabled &&
      Services.prefs.getBoolPref("floorp.workspaces.enabled", true);
  }
  set enabled(value: boolean) {
    super.enabled = value;
  }
  async _processIncoming(): Promise<void> {
    await super._processIncoming();
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
    const result = await super._maybeReconcile(record);
    await ready();
    await mutate(() => {
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
    const failed = await super._applyRecords(records, telemetry) as string[];
    await ready();
    await mutate(() => {
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
    await ready();
    await waitForProviders();
    await mutate(async () => {
      for (const [id, tabs] of Object.entries(state().pendingCloses)) {
        const document = state().records[id];
        if (document && state().selected.includes(id) && !retired.has(id)) {
          await apply({ ...document, tabs: [...document.tabs, ...tabs] });
        }
        delete state().pendingCloses[id];
      }
      file.saveSoon();
    });
  }
  async getChangedIDs(): Promise<Record<string, number>> {
    await ready();
    const tracked = await this._tracker.getChangedIDs() as Record<
      string,
      number
    >;
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
    await accountReset;
    const { getFxAccountsSingleton } = ChromeUtils.importESModule(
      "resource://gre/modules/FxAccounts.sys.mjs",
    );
    const user = await getFxAccountsSingleton().getSignedInUser();
    if (!user?.uid) {
      throw new Error("Workspace Sync requires a signed-in Mozilla Account");
    }
    await ready();
    if (state().accountUid && state().accountUid !== user.uid) {
      await disarmAccount();
    }
    await mutate(() => {
      state().accountUid = user.uid;
      file.saveSoon();
    });
    await FloorpWorkspacesSync.capture();
    await trackedChanges;
    await super._syncStartup();
  }
  async _reconcile(record: FloorpWorkspaceRecord): Promise<boolean> {
    await ready();
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
      await mutate(() => tombstone(record.id));
      this._modified.set(record.id, Date.now() / 1000);
      return false;
    }
    return true;
  }
  async _onRecordsWritten(succeeded: string[]): Promise<void> {
    await ready();
    await mutate(() => {
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
