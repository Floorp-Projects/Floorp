// SPDX-License-Identifier: MPL-2.0

import {
  isWorkspaceSyncId,
  liveWorkspaceSyncTabs,
  mergeWorkspaceSyncDocuments,
  migrateWorkspaceSyncDocumentToV2,
  parseWorkspaceSyncDocument,
  updateWorkspaceSyncDocument,
  WORKSPACE_SYNC_CHANGED,
  WORKSPACE_SYNC_V2_COLLECTION as WORKSPACE_SYNC_COLLECTION,
  WORKSPACE_SYNC_V2_ENGINE_PREF as WORKSPACE_SYNC_ENGINE_PREF,
} from "../../common/workspace-sync.ts";
import {
  parseWorkspaceSyncRecord,
  workspaceContainerRecord,
  workspaceMetadataRecord,
  workspaceTabRecord,
} from "../../common/workspace-sync-record.ts";
import { sameWorkspaceContainerRef } from "../../common/workspace-container.ts";
import {
  WORKSPACE_CONTAINER_ASSOCIATION_CHANGED,
  WORKSPACE_CONTAINER_BINDINGS_CHANGED,
} from "../../common/workspace-container-bindings.ts";
import type { WorkspaceMigrationService } from "./WorkspaceContainerMigration.sys.mts";
import type { WorkspaceMigrationRecord } from "../../common/workspace-container-migration.ts";
import { canonicalWorkspaceMigrationRecord } from "../../common/workspace-container-migration.ts";
import type { WorkspaceSyncRecordValue } from "../../common/workspace-sync-record-types.ts";
import type {
  WorkspaceContainerDefinition,
  WorkspaceContainerRef,
} from "../../common/workspace-container-types.ts";
import type {
  WorkspaceSyncDocument,
  WorkspaceSyncProvider,
  WorkspaceSyncSnapshot,
  WorkspaceSyncTab,
} from "../../common/workspace-sync-types.ts";

// Share the profile coordinator with browser chrome. Bundling its implementation
// into this engine would create a second mapping table and duplicate observers.
const { WorkspaceContainerBindings } = ChromeUtils.importESModule(
  "resource://noraneko/modules/workspaces/WorkspaceContainerBindings.sys.mjs",
) as typeof import("./WorkspaceContainerBindings.sys.mts");
const { WorkspaceContainerMigration } = ChromeUtils.importESModule(
  "resource://noraneko/modules/workspaces/WorkspaceContainerMigration.sys.mjs",
) as typeof import("./WorkspaceContainerMigration.sys.mts");

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
  version: 2;
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
  containerPending: Record<
    string,
    { tabs: string[]; defaultContainer: boolean }
  >;
  protectedTabs: Record<string, string[]>;
  reopenOperations: Record<string, { workspaceId: string; tabId: string }>;
  migrationId: string | null;
  migrationResendPending: boolean;
  bootstrapPending: boolean;
}

function emptyState(accountUid: string | null = null): SyncState {
  return {
    version: 2,
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
    containerPending: {},
    protectedTabs: {},
    reopenOperations: {},
    migrationId: null,
    migrationResendPending: false,
    bootstrapPending: false,
  };
}
const providers = new Set<WorkspaceSyncProvider>();
const retired = new Set<string>();
const captureErrors = new Map<string, string>();
const statePath = PathUtils.join(
  PathUtils.profileDir,
  "floorp-workspaces-sync-v2.json",
);
const file = new JSONFile({
  path: statePath,
  dataPostProcessor: (value: unknown): SyncState => {
    const raw = value as Partial<SyncState> | null;
    if (raw?.version !== undefined && raw.version !== 2) {
      throw new Error("Unsupported workspace sync storage version");
    }
    const result = emptyState(
      typeof raw?.accountUid === "string" ? raw.accountUid : null,
    );
    if (
      raw?.migrationId !== undefined && raw.migrationId !== null &&
      !isWorkspaceSyncId(raw.migrationId)
    ) {
      throw new Error("Invalid workspace migration state");
    }
    result.migrationId = raw?.migrationId ?? null;
    if (
      raw?.migrationResendPending !== undefined &&
      typeof raw.migrationResendPending !== "boolean"
    ) {
      throw new Error("Invalid workspace migration resend state");
    }
    result.migrationResendPending = raw?.migrationResendPending === true;
    result.bootstrapPending = raw?.bootstrapPending === true;
    for (const [id, document] of Object.entries(raw?.records ?? {})) {
      if (!isWorkspaceSyncId(id) || document.workspaceId !== id) {
        throw new Error("Stored workspace ID mismatch");
      }
      // Validate each independently sized wire entity, rather than limiting the
      // aggregate profile cache or retaining closed-tab payloads indefinitely.
      parseWorkspaceSyncRecord(workspaceMetadataRecord(document));
      for (const tab of document.tabs) {
        if (!tab.deleted) {
          parseWorkspaceSyncRecord(
            workspaceTabRecord(id, tab, document.version),
          );
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
          version: 2,
          workspaceId: id,
          name: "",
          icon: null,
          defaultContainer: { kind: "legacy-unresolved" },
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
      try {
        if (!isWorkspaceSyncId(id) || snapshot?.workspaceId !== id) {
          throw new Error("Invalid pending workspace baseline");
        }
        result.pendingBaselines[id] = validatedSnapshot(snapshot);
      } catch {
        // A baseline is only an observation of local UI, not desired remote
        // data. Quarantine it without discarding the pending-apply marker: a
        // missing baseline preserves additions that have never reached the UI.
      }
    }
    for (const [id, pending] of Object.entries(raw?.containerPending ?? {})) {
      if (
        !isWorkspaceSyncId(id) || !Array.isArray(pending.tabs) ||
        !pending.tabs.every(isWorkspaceSyncId) ||
        typeof pending.defaultContainer !== "boolean"
      ) {
        throw new Error("Invalid pending workspace container state");
      }
      result.containerPending[id] = {
        tabs: [...pending.tabs],
        defaultContainer: pending.defaultContainer,
      };
    }
    for (const [id, tabs] of Object.entries(raw?.protectedTabs ?? {})) {
      if (
        !isWorkspaceSyncId(id) || !Array.isArray(tabs) ||
        !tabs.every(isWorkspaceSyncId)
      ) {
        throw new Error("Invalid workspace container closure protection");
      }
      result.protectedTabs[id] = [...tabs];
    }
    for (
      const [token, operation] of Object.entries(raw?.reopenOperations ?? {})
    ) {
      if (
        !isWorkspaceSyncId(token) ||
        !isWorkspaceSyncId(operation.workspaceId) ||
        !isWorkspaceSyncId(operation.tabId)
      ) throw new Error("Invalid workspace tab replacement");
      result.reopenOperations[token] = { ...operation };
    }
    return result;
  },
});
let fileLoaded: Promise<void> | undefined;
let loaded: Promise<void> | undefined;
let initialized = false;
let trackedChanges = Promise.resolve();
const state = (): SyncState => file.data as SyncState;
// Loading bytes is independent of account initialization: startOver can await
// this boundary while cancelling an in-flight ready(), without a promise cycle.
const loadFile = (): Promise<void> => fileLoaded ??= file.load();
const ready = async (): Promise<void> => {
  const expected = accountEpoch;
  await accountReset;
  assertAccountEpoch(expected);
  if (!loaded) {
    const initializing = (async () => {
      await loadFile();
      assertAccountEpoch(expected);
      await WorkspaceContainerBindings.ready(state().accountUid, deviceId());
      assertAccountEpoch(expected);
      await mutate(async () => {
        assertAccountEpoch(expected);
        for (const id of state().retired) retired.add(id);
        const interrupted = Object.values(state().reopenOperations);
        if (interrupted.length) {
          for (const { workspaceId, tabId } of interrupted) {
            const pending = state().containerPending[workspaceId] ??= {
              tabs: [],
              defaultContainer: false,
            };
            if (!pending.tabs.includes(tabId)) pending.tabs.push(tabId);
            state().protectedTabs[workspaceId] = [
              ...new Set([
                ...(state().protectedTabs[workspaceId] ?? []),
                tabId,
              ]),
            ];
          }
          // Previous-process tokens cannot finish. Keep desired tabs available
          // for recovery and release their ordinary-close suppression.
          state().reopenOperations = {};
          await persistState();
          assertAccountEpoch(expected);
        }
        initialized = true;
      });
    })();
    loaded = initializing;
    initializing.catch(() => {
      if (loaded === initializing) loaded = undefined;
    });
  }
  await loaded;
  assertAccountEpoch(expected);
};
// Invalidate synchronously: Firefox startOver does not wait for an active sync.
let accountEpoch = 0;
let accountAbort = new AbortController();
let providerAbort = new AbortController();
function workspacesEnabled(): boolean {
  return Services.prefs.getBoolPref("floorp.workspaces.enabled", true);
}
function assertWorkspacesEnabled(): void {
  if (!workspacesEnabled()) {
    throw new Error("Workspace Sync is disabled");
  }
}
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
/** JSONFile._save handles IO errors internally; verify the durable boundary. */
async function persistState(): Promise<void> {
  const expected = JSON.stringify(state());
  await file._save();
  if (JSON.stringify(await IOUtils.readJSON(statePath)) !== expected) {
    throw new Error("Workspace sync state was not durably saved");
  }
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
function referencedDefinitions(): WorkspaceContainerDefinition[] {
  const ids = new Set<string>();
  for (const id of state().selected) {
    const document = state().records[id];
    if (!document || retired.has(id) || captureErrors.has(id)) continue;
    const refs = [
      document.defaultContainer,
      ...document.tabs.filter((tab) => !tab.deleted).map((tab) =>
        tab.container
      ),
    ];
    for (const ref of refs) {
      if (ref?.kind === "container") ids.add(ref.containerId);
    }
  }
  return [...ids].map((id) => WorkspaceContainerBindings.getDefinition(id))
    .filter((definition): definition is WorkspaceContainerDefinition =>
      Boolean(definition)
    );
}

/** Read the previous cache once for migration; never modify its file or collection. */
async function legacyMigrationInput(accountUid: string): Promise<{
  documents: WorkspaceSyncDocument[];
  metadataKnown: string[];
  retired: string[];
  selected: string[];
  excluded: string[];
}> {
  const empty = {
    documents: [],
    metadataKnown: [],
    retired: [],
    selected: [],
    excluded: [],
  };
  let value: unknown;
  try {
    value = await IOUtils.readJSON(
      PathUtils.join(PathUtils.profileDir, "floorp-workspaces-sync.json"),
    );
  } catch (error) {
    if (error instanceof DOMException && error.name === "NotFoundError") {
      return empty;
    }
    throw error;
  }
  if (!value || typeof value !== "object") {
    throw new Error("Invalid legacy workspace cache");
  }
  const raw = value as Partial<Omit<SyncState, "version">> & {
    version?: number;
  };
  if (raw.version !== 1) throw new Error("Unsupported legacy workspace cache");
  if (raw.accountUid && raw.accountUid !== accountUid) return empty;
  const selected = (raw.selected ?? []).filter(isWorkspaceSyncId);
  const excluded = (raw.excluded ?? []).filter(isWorkspaceSyncId);
  const documents: WorkspaceSyncDocument[] = [];
  for (const id of selected) {
    if (excluded.includes(id) || !raw.records?.[id]) continue;
    const document = parseWorkspaceSyncDocument(raw.records[id], 1);
    if (document.workspaceId !== id) {
      throw new Error("Legacy workspace ID mismatch");
    }
    // Only a currently observed local tab can supply its old missing context.
    // Missing remote-only tabs retain legacy-unresolved and require user choice.
    const observed = captureSnapshot(id);
    const tabs = Object.fromEntries(
      (observed?.tabs ?? []).filter((tab) => tab.container).map((
        tab,
      ) => [tab.id, tab.container!]),
    );
    documents.push(migrateWorkspaceSyncDocumentToV2(document, {
      defaultContainer: observed?.defaultContainer,
      tabs,
    }));
  }
  await WorkspaceContainerBindings.flush();
  return {
    documents,
    metadataKnown: (raw.metadataKnown ?? []).filter(isWorkspaceSyncId),
    retired: raw.accountUid === accountUid
      ? [
        ...new Set([...(raw.retired ?? []), ...Object.keys(raw.deleted ?? {})]),
      ].filter(isWorkspaceSyncId)
      : [],
    selected,
    excluded,
  };
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
  if (!workspacesEnabled()) {
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
Services.obs.addObserver(() => {
  // Native enablement reconciliation precedes engine startup. A new collection
  // has no meta/global entry yet, so preserve the configured opt-in until the
  // migration coordinator can initialize that entry. Repeat after failures.
  if (
    initialized && !state().migrationId && workspacesEnabled() &&
    Services.prefs.getBoolPref(WORKSPACE_SYNC_ENGINE_PREF, true)
  ) {
    Services.prefs.setBoolPref(
      `services.sync.engineStatusChanged.${WORKSPACE_SYNC_COLLECTION}`,
      true,
    );
  }
}, "weave:service:sync:start");
Services.prefs.addObserver("floorp.workspaces.enabled", () => {
  registrationChanged();
  if (!workspacesEnabled()) {
    // Native Sync only checks enabled before starting its network request.
    // Abort UI work already in flight; rejected incoming IDs retain native and
    // persisted retry markers instead of being acknowledged and forgotten.
    providerAbort.abort();
    return;
  }
  if (providerAbort.signal.aborted) providerAbort = new AbortController();
  // A completed native download may have advanced lastSync while disabled.
  // Refetch on the next Sync as well as retaining failed incoming IDs.
  Services.prefs.setStringPref(
    `services.sync.${WORKSPACE_SYNC_COLLECTION}.lastSync`,
    "0",
  );
  if (initialized) {
    FloorpWorkspacesSync.capture().catch((error: unknown) =>
      console.error("[WorkspacesSync] Could not resume pending work", error)
    );
  }
});
Services.obs.addObserver(() => {
  for (const waiter of [...registrationWaiters]) {
    waiter.reject(new Error("Browser is shutting down"));
  }
}, "quit-application-granted");

interface ContainerRemovalOperation {
  operationId: string;
  userContextIds: number[];
  outcome?: "completed" | "cancelled";
  blockers: Promise<void>[];
}
const activeContainerRemovals = new Map<string, Set<string>>();
Services.obs.addObserver((subject) => {
  const operation =
    (subject as { wrappedJSObject?: ContainerRemovalOperation } | null)
      ?.wrappedJSObject;
  if (!operation || !Array.isArray(operation.blockers)) return;
  const expected = accountEpoch;
  const closingTabs = new Set<string>();
  activeContainerRemovals.set(operation.operationId, closingTabs);
  operation.blockers.push((async () => {
    await ready();
    await waitForProviders(accountAbort.signal);
    await mutate(async () => {
      assertAccountEpoch(expected);
      captureNow();
      const logicalIds = new Set(
        WorkspaceContainerBindings.containerIdsForLocalIds(
          operation.userContextIds,
        ),
      );
      const actual = [...providers].filter((provider) =>
        !provider.window.closed
      )
        .map((provider) =>
          provider.containerTabIds?.(operation.userContextIds) ?? {}
        );
      for (const id of state().selected) {
        const document = state().records[id];
        if (!document) continue;
        if (captureErrors.has(id)) {
          throw new Error(
            "Cannot protect an invalid workspace before container removal",
          );
        }
        const affected = new Set(actual.flatMap((tabs) => tabs[id] ?? []));
        for (const tabId of affected) closingTabs.add(`${id}/${tabId}`);
        for (const tab of document.tabs) {
          if (
            !tab.deleted && tab.container?.kind === "container" &&
            logicalIds.has(tab.container.containerId)
          ) affected.add(tab.id);
        }
        const defaultAffected =
          document.defaultContainer?.kind === "container" &&
          logicalIds.has(document.defaultContainer.containerId);
        if (!affected.size && !defaultAffected) continue;
        state().protectedTabs[id] = [
          ...new Set([...(state().protectedTabs[id] ?? []), ...affected]),
        ];
        const pending = state().containerPending[id] ??= {
          tabs: [],
          defaultContainer: false,
        };
        pending.tabs = [...new Set([...pending.tabs, ...affected])];
        pending.defaultContainer ||= defaultAffected;
        changed(id);
      }
      await WorkspaceContainerBindings.flush();
      await persistState();
    });
  })());
}, "floorp-container-removal-preparing");
Services.obs.addObserver((subject) => {
  const operation =
    (subject as { wrappedJSObject?: ContainerRemovalOperation } | null)
      ?.wrappedJSObject;
  if (!operation || !Array.isArray(operation.blockers)) return;
  operation.blockers.push((async () => {
    // Observer order is not guaranteed. Let all synchronous observers enqueue
    // completion, then await the binding journal before retrying pending tabs.
    await Promise.resolve();
    try {
      await WorkspaceContainerBindings.flush();
    } finally {
      // Native closure has ended even if its completion save fails. Keep the
      // durable missing-tab protection, but do not suppress later user closes.
      activeContainerRemovals.delete(operation.operationId);
    }
    if (initialized) await FloorpWorkspacesSync.capture();
  })());
}, "floorp-container-removal-finished");
const pendingContainerAssociations = new Set<string>();
let bindingRetryScheduled = false;
function scheduleBindingRetry(): void {
  if (!initialized || bindingRetryScheduled) return;
  bindingRetryScheduled = true;
  setTimeout(() => {
    bindingRetryScheduled = false;
    if (activeContainerRemovals.size) return;
    FloorpWorkspacesSync.capture().catch((error: unknown) =>
      console.error("[WorkspacesSync] Could not retry container apply", error)
    );
  }, 0);
}
Services.obs.addObserver(
  scheduleBindingRetry,
  WORKSPACE_CONTAINER_BINDINGS_CHANGED,
);
Services.obs.addObserver((_subject, _topic, id) => {
  if (!isWorkspaceSyncId(id)) return;
  pendingContainerAssociations.add(id);
  scheduleBindingRetry();
}, WORKSPACE_CONTAINER_ASSOCIATION_CHANGED);

function markPendingContainerAssociations(): boolean {
  if (!pendingContainerAssociations.size) return false;
  const changedIds = new Set(pendingContainerAssociations);
  pendingContainerAssociations.clear();
  let marked = false;
  for (const id of state().selected) {
    const document = state().records[id];
    if (!document || retired.has(id)) continue;
    const matches = (ref?: WorkspaceContainerRef) =>
      ref?.kind === "container" && changedIds.has(ref.containerId);
    const defaultChanged = matches(document.defaultContainer);
    const tabs = document.tabs.filter((tab) =>
      !tab.deleted && matches(tab.container)
    ).map((tab) => tab.id);
    if (!defaultChanged && !tabs.length) continue;
    const pending = state().containerPending[id] ??= {
      tabs: [],
      defaultContainer: false,
    };
    pending.defaultContainer ||= defaultChanged;
    pending.tabs = [...new Set([...pending.tabs, ...tabs])];
    marked = true;
    changed(id);
  }
  return marked;
}

async function apply(
  document: WorkspaceSyncDocument,
  expected = accountEpoch,
): Promise<void> {
  assertAccountEpoch(expected);
  assertWorkspacesEnabled();
  const signal = providerAbort.signal;
  const targets = [...providers].filter((provider) => !provider.window.closed);
  if (!targets.length) throw new Error("No workspace window is available");
  const known = new Set(
    targets.flatMap((provider) => provider.tabIds(document.workspaceId)),
  );
  const missing = new Set(
    liveWorkspaceSyncTabs(document).map((tab) => tab.id).filter((id) =>
      !known.has(id)
    ),
  );
  let applied = false;
  const pendingTabs = new Set<string>();
  let pendingDefault = false;
  for (let index = 0; index < targets.length; index++) {
    if (!targets[index].window.closed) {
      assertAccountEpoch(expected);
      assertWorkspacesEnabled();
      const result = await targets[index].apply(
        document,
        !applied ? missing : new Set(),
        signal,
      );
      for (const id of result?.pendingContainerTabIds ?? []) {
        pendingTabs.add(id);
      }
      pendingDefault ||= result?.pendingDefaultContainer ?? false;
      assertAccountEpoch(expected);
      assertWorkspacesEnabled();
      applied = true;
    }
  }
  if (!applied) throw new Error("No workspace window is available");
  const id = document.workspaceId;
  if (pendingTabs.size || pendingDefault) {
    state().containerPending[id] = {
      tabs: [...pendingTabs],
      defaultContainer: pendingDefault,
    };
  } else delete state().containerPending[id];
  const materialized = new Set(targets.flatMap((target) => target.tabIds(id)));
  const live = new Set(
    document.tabs.filter((tab) => !tab.deleted).map((tab) => tab.id),
  );
  const stillProtected = (state().protectedTabs[id] ?? []).filter((tabId) =>
    live.has(tabId) && (!materialized.has(tabId) || pendingTabs.has(tabId))
  );
  if (stillProtected.length) state().protectedTabs[id] = stillProtected;
  else delete state().protectedTabs[id];
  await WorkspaceContainerBindings.flush();
  await persistState();
}
function captureSnapshot(
  id: string,
  seenIds = new Set<string>(Object.keys(state().records)),
): WorkspaceSyncSnapshot | null {
  const snapshots = [...providers].filter((provider) => !provider.window.closed)
    .map((provider) => provider.capture(id, seenIds))
    .filter((value): value is WorkspaceSyncSnapshot => value !== null);
  if (!snapshots.length) return null;
  const document = state().records[id];
  const protectedTabs = new Set([
    ...(state().protectedTabs[id] ?? []),
    ...(state().containerPending[id]?.tabs ?? []),
    ...snapshots.flatMap((snapshot) => snapshot.protectedTabIds ?? []),
  ]);
  for (const tab of document?.tabs ?? []) {
    if (tab.deleted || !tab.container) continue;
    if (WorkspaceContainerBindings.lookup(tab.container).status === "pending") {
      protectedTabs.add(tab.id);
    }
  }
  for (const operation of Object.values(state().reopenOperations)) {
    if (operation.workspaceId === id) protectedTabs.add(operation.tabId);
  }
  return {
    ...snapshots[0],
    orderChanged: snapshots.some((snapshot) => snapshot.orderChanged !== false),
    tabs: snapshots.flatMap((value) => value.tabs),
    protectedTabIds: [...protectedTabs],
  };
}
function documentSnapshot(
  document: WorkspaceSyncDocument,
): WorkspaceSyncSnapshot {
  return {
    version: document.version,
    workspaceId: document.workspaceId,
    name: document.name,
    icon: document.icon,
    ...(document.defaultContainer
      ? { defaultContainer: document.defaultContainer }
      : {}),
    orderChanged: false,
    tabs: liveWorkspaceSyncTabs(document).map((
      { id, url, title, pinned, container },
    ) => ({
      id,
      url,
      title,
      pinned,
      ...(container ? { container } : {}),
    })),
  };
}
function validatedSnapshot(
  snapshot: WorkspaceSyncSnapshot,
): WorkspaceSyncSnapshot {
  const document = updateWorkspaceSyncDocument(
    null,
    snapshot,
    snapshot.workspaceId,
    0,
  );
  parseWorkspaceSyncRecord(workspaceMetadataRecord(document));
  for (const tab of document.tabs) {
    if (!tab.deleted) {
      parseWorkspaceSyncRecord(workspaceTabRecord(document.workspaceId, tab));
    }
  }
  return {
    ...documentSnapshot(document),
    orderChanged: snapshot.orderChanged,
    protectedTabIds: snapshot.protectedTabIds,
  };
}
function pendingSnapshot(
  desired: WorkspaceSyncDocument,
  observed: WorkspaceSyncSnapshot,
): WorkspaceSyncSnapshot {
  const snapshot = documentSnapshot(desired);
  snapshot.protectedTabIds = observed.protectedTabIds;
  const baseline = state().pendingBaselines[desired.workspaceId];
  if (!baseline) return snapshot;
  if (observed.name !== baseline.name) snapshot.name = observed.name;
  if (observed.icon !== baseline.icon) snapshot.icon = observed.icon;
  if (
    observed.defaultContainer &&
    !sameWorkspaceContainerRef(
      observed.defaultContainer,
      baseline.defaultContainer,
    )
  ) {
    snapshot.defaultContainer = observed.defaultContainer;
  }
  const before = new Map(baseline.tabs.map((tab) => [tab.id, tab]));
  const current = new Map(observed.tabs.map((tab) => [tab.id, tab]));
  const ids = new Set(snapshot.tabs.map((tab) => tab.id));
  snapshot.tabs = snapshot.tabs.filter((tab) => {
    // A tab observed before the failed apply can still be explicitly closed by
    // the user. A missing remote addition was never observed, so retain it.
    return !before.has(tab.id) || current.has(tab.id) ||
      observed.protectedTabIds?.includes(tab.id);
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
      ...(local.container &&
          !sameWorkspaceContainerRef(local.container, previous.container)
        ? { container: local.container }
        : {}),
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
  if (!workspacesEnabled() || state().bootstrapPending) return;
  // A rebind changes the native destination of an existing logical reference.
  // Mark it before observing old physical defaults, without discarding unrelated
  // unsent URL/title/order edits. Source-side definition capture has no such event.
  markPendingContainerAssociations();
  const seenIds = new Set<string>(Object.keys(state().records));
  for (const id of state().selected) {
    try {
      const observed = captureSnapshot(id, seenIds);
      if (!observed) continue;
      // Validate the physical observation even when a missing pending baseline
      // means pendingSnapshot will intentionally retain the desired document.
      validatedSnapshot(observed);
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
          `services.sync.${WORKSPACE_SYNC_COLLECTION}.lastSync`,
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
  deferred?: Set<string>,
): Promise<void> {
  assertAccountEpoch(expected);
  const id = document.workspaceId;
  if (!state().pendingApplies.includes(id)) state().pendingApplies.push(id);
  const baseline = captureSnapshot(id);
  if (baseline) state().pendingBaselines[id] = validatedSnapshot(baseline);
  changed(id);
  if (deferred) {
    deferred.add(id);
    return;
  }
  await apply(document, expected);
  assertAccountEpoch(expected);
  if (completesClosures || !state().pendingCloses[id]?.length) {
    state().pendingApplies = state().pendingApplies.filter((value) =>
      value !== id
    );
    delete state().pendingBaselines[id];
  } else {
    const current = captureSnapshot(id);
    if (current) state().pendingBaselines[id] = validatedSnapshot(current);
  }
  changed(id);
}
async function retryPendingApplies(expected: number): Promise<void> {
  if (!workspacesEnabled()) return;
  const pendingContainers = Object.entries(state().containerPending).filter((
    [, pending],
  ) => pending.defaultContainer || pending.tabs.length).map(([id]) => id);
  const protectedWorkspaces = Object.entries(state().protectedTabs).filter((
    [, tabs],
  ) => tabs.length).map(([id]) => id);
  for (
    const id of new Set([
      ...state().pendingApplies,
      ...pendingContainers,
      ...protectedWorkspaces,
    ])
  ) {
    assertAccountEpoch(expected);
    const document = state().records[id];
    if (document && state().selected.includes(id) && !retired.has(id)) {
      // Only a completed incoming pass can decide whether a later parent
      // tombstone cancels these closures. Ordinary capture must not flush them.
      if (state().pendingCloses[id]?.length || captureErrors.has(id)) continue;
      await applySelected(document, expected);
    } else if (!document || retired.has(id)) {
      state().pendingApplies = state().pendingApplies.filter((value) =>
        value !== id
      );
      delete state().pendingBaselines[id];
      delete state().containerPending[id];
      delete state().protectedTabs[id];
      file.saveSoon();
    }
  }
}
function disarmAccount(): Promise<void> {
  const expected = ++accountEpoch;
  let resolveReset!: () => void;
  let rejectReset!: (error: unknown) => void;
  const reset = new Promise<void>((resolve, reject) => {
    resolveReset = resolve;
    rejectReset = reject;
  });
  // Publish the barrier before abort callbacks or side services can schedule
  // another API call. Every ready() waits for all durable clears below.
  accountReset = reset;
  initialized = false;
  loaded = undefined;
  WorkspaceContainerBindings.invalidateAccount();
  accountAbort.abort();
  accountAbort = new AbortController();
  providerAbort.abort();
  providerAbort = new AbortController();
  // Queue the state clear before any later capture; do not call ready() here,
  // because it now waits for this reset and may itself have been cancelled.
  const clearState = mutate(async () => {
    await loadFile();
    assertAccountEpoch(expected);
    file.data = emptyState();
    retired.clear();
    captureErrors.clear();
    sentTombstones.clear();
    await persistState();
    assertAccountEpoch(expected);
  });
  const clearBindings = WorkspaceContainerBindings.resetAccount();
  const clearMigration = WorkspaceContainerMigration.resetAccount();
  Promise.all([clearState, clearBindings, clearMigration]).then(() => {
    assertAccountEpoch(expected);
    Services.obs.notifyObservers(
      null as unknown as nsISupports,
      WORKSPACE_SYNC_CHANGED,
    );
  }).then(resolveReset, rejectReset);
  return reset;
}
let accountReset = Promise.resolve();
Services.obs.addObserver(() => {
  accountReset = disarmAccount();
  accountReset.catch(console.error);
}, "weave:service:start-over");

/** Profile-scoped state; site data and local container numbers never enter transport. */
export const FloorpWorkspacesSync = {
  ready,
  isContainerSyncEnabled(): boolean {
    return true;
  },
  isContainerApplyPending(id: string): boolean {
    return initialized && Boolean(state().containerPending[id]);
  },
  isContainerDefaultPending(id: string): boolean {
    return initialized &&
      state().containerPending[id]?.defaultContainer === true;
  },
  getSyncedWorkspace(id: string): WorkspaceSyncDocument | null {
    if (!initialized || retired.has(id)) return null;
    const document = state().records[id];
    return document ? structuredClone(document) : null;
  },
  async resolveLegacyContainers(
    workspaceId: string,
    target: number | "new",
    scope: "default" | "tabs" | "all",
  ): Promise<void> {
    const expected = accountEpoch;
    await ready();
    await waitForProviders(accountAbort.signal);
    await mutate(async () => {
      assertAccountEpoch(expected);
      const document = state().records[workspaceId];
      if (
        !document || !state().selected.includes(workspaceId) ||
        retired.has(workspaceId)
      ) {
        throw new Error("Workspace is no longer selected for container setup");
      }
      const changeDefault = scope !== "tabs" &&
        document.defaultContainer?.kind === "legacy-unresolved";
      const tabs = scope === "default"
        ? []
        : document.tabs.filter((tab) =>
          !tab.deleted && tab.container?.kind === "legacy-unresolved"
        );
      if (!changeDefault && !tabs.length) return;
      const now = Date.now();
      let ref: WorkspaceContainerRef;
      if (target === "new") {
        const definition: WorkspaceContainerDefinition = {
          version: 2,
          kind: "container",
          containerId: Services.uuid.generateUUID().toString().slice(1, -1),
          name: (document.name || "Workspace").slice(0, 256),
          color: "blue",
          icon: "fingerprint",
          modifiedAt: now,
          modifiedBy: deviceId(),
        };
        await WorkspaceContainerBindings.receiveDefinitions([definition]);
        ref = { kind: "container", containerId: definition.containerId };
      } else {
        ref = WorkspaceContainerBindings.captureReference(target).ref;
      }
      assertAccountEpoch(expected);
      if (changeDefault) {
        document.defaultContainer = ref;
        document.metadata = { modifiedAt: now, modifiedBy: deviceId() };
        changed(workspaceId, true);
      }
      for (const tab of tabs) {
        tab.container = ref;
        tab.modifiedAt = now;
        tab.modifiedBy = deviceId();
        changed(tab.id, true);
      }
      if (!state().pendingApplies.includes(workspaceId)) {
        state().pendingApplies.push(workspaceId);
      }
      await WorkspaceContainerBindings.flush();
      await persistState();
      // Persist the chosen logical identity before native creation. Failures
      // then remain recoverable through its existing creation journal/setup UI.
      if (target === "new" && ref.kind === "container") {
        await WorkspaceContainerBindings.create(ref.containerId);
      }
      assertAccountEpoch(expected);
      await applySelected(document, expected);
    });
  },
  async noteTabClosed(
    workspaceId: string,
    tabId: string,
    reason: "close" | "move" = "close",
  ): Promise<void> {
    const expected = accountEpoch;
    // Capture provenance synchronously with TabClose. The removal completion
    // can release its native token before this queued mutation gets the lock.
    const removalClosure = reason === "close" &&
      [...activeContainerRemovals.values()].some((ids) =>
        ids.has(`${workspaceId}/${tabId}`)
      );
    await ready();
    await mutate(async () => {
      assertAccountEpoch(expected);
      if (!state().selected.includes(workspaceId) || removalClosure) return;
      const document = state().records[workspaceId];
      const tab = document?.tabs.find((item) =>
        item.id === tabId && !item.deleted
      );
      if (!tab) return;
      state().protectedTabs[workspaceId] =
        (state().protectedTabs[workspaceId] ?? []).filter((id) => id !== tabId);
      const pending = state().containerPending[workspaceId];
      if (pending) {
        pending.tabs = pending.tabs.filter((id) => id !== tabId);
        if (!pending.tabs.length && !pending.defaultContainer) {
          delete state().containerPending[workspaceId];
        }
      }
      if (!state().protectedTabs[workspaceId]?.length) {
        delete state().protectedTabs[workspaceId];
      }
      tab.deleted = true;
      tab.modifiedAt = Date.now();
      tab.modifiedBy = deviceId();
      retire(tabId);
      tombstone(tabId);
      await persistState();
    });
  },
  async beginContainerReopen(
    workspaceId: string,
    tabId: string,
  ): Promise<string> {
    const expected = accountEpoch;
    await ready();
    return await mutate(async () => {
      assertAccountEpoch(expected);
      if (
        !state().selected.includes(workspaceId) ||
        !state().records[workspaceId]?.tabs.some((tab) =>
          tab.id === tabId && !tab.deleted
        )
      ) {
        throw new Error("Workspace tab is no longer available for replacement");
      }
      const token = Services.uuid.generateUUID().toString().slice(1, -1);
      state().reopenOperations[token] = { workspaceId, tabId };
      await persistState();
      return token;
    });
  },
  async finishContainerReopen(token: string, success: boolean): Promise<void> {
    const expected = accountEpoch;
    await ready();
    await mutate(async () => {
      assertAccountEpoch(expected);
      const operation = state().reopenOperations[token];
      if (!operation) return;
      delete state().reopenOperations[token];
      if (!success) {
        const pending = state().containerPending[operation.workspaceId] ??= {
          tabs: [],
          defaultContainer: false,
        };
        if (!pending.tabs.includes(operation.tabId)) {
          pending.tabs.push(operation.tabId);
        }
      }
      await persistState();
      changed(operation.workspaceId);
    });
  },
  isSelected(id: string): boolean {
    return initialized && state().selected.includes(id);
  },
  getSyncedTab(
    workspaceId: string,
    tabId: string,
  ): WorkspaceSyncSnapshot["tabs"][number] | null {
    if (!initialized || retired.has(workspaceId) || retired.has(tabId)) {
      return null;
    }
    const tab = state().records[workspaceId]?.tabs.find((tab) =>
      tab.id === tabId && !tab.deleted
    );
    if (!tab) return null;
    const { id, url, title, pinned, container } = tab;
    return { id, url, title, pinned, ...(container ? { container } : {}) };
  },
  errorFor(id: string): string | null {
    if (retired.has(id)) return "deleted-workspace";
    if (initialized && state().containerPending[id]) return "container-pending";
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
          if (state().records[id] && !captureErrors.has(id)) {
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
          `services.sync.${WORKSPACE_SYNC_COLLECTION}.lastSync`,
          "0",
        );
        // Existing accounts have no meta/global entry for this new engine, so
        // Gecko can disable it during reconciliation despite our default=true.
        // Selecting a workspace is an explicit local opt-in: enable transport
        // and mark that choice so the next reconciliation preserves it.
        Services.prefs.setBoolPref(WORKSPACE_SYNC_ENGINE_PREF, true);
        Services.prefs.setBoolPref(
          `services.sync.engineStatusChanged.${WORKSPACE_SYNC_COLLECTION}`,
          true,
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
      if (markPendingContainerAssociations()) await persistState();
      captureNow();
      await retryPendingApplies(expected);
      await WorkspaceContainerBindings.flush();
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
      delete state().containerPending[id];
      delete state().protectedTabs[id];
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
    assertWorkspacesEnabled();
    if (this.engine.migrationPending) {
      return Object.fromEntries(
        this.engine.pendingMigrationRecords().map((
          record: WorkspaceMigrationRecord,
        ) => [record.id, true]),
      );
    }
    const ids = [...Object.keys(state().deleted)];
    ids.push(
      ...this.engine.pendingMigrationRecords().map((
        record: WorkspaceMigrationRecord,
      ) => record.id),
    );
    ids.push(
      ...referencedDefinitions().map((definition) => definition.containerId),
    );
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
    assertWorkspacesEnabled();
    if (WorkspaceContainerBindings.getDefinition(id)) return true;
    if (
      this.engine.pendingMigrationRecords().some((
        record: WorkspaceMigrationRecord,
      ) => record.id === id && !record.deleted)
    ) return true;
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
    assertWorkspacesEnabled();
    const record = new FloorpWorkspaceRecord(collection, id);
    const staged = this.engine.pendingMigrationRecords().find((
      value: WorkspaceMigrationRecord,
    ) => value.id === id);
    if (staged) {
      if (staged.deleted) record.deleted = true;
      else record.entity = staged.entity!;
      this.engine.rememberOutgoing(record);
      return record;
    }
    if (Object.hasOwn(state().deleted, id)) {
      record.deleted = true;
      sentTombstones.set(id, state().deleted[id]);
      this.engine.rememberOutgoing(record);
      return record;
    }
    const definition = WorkspaceContainerBindings.getDefinition(id);
    if (
      definition &&
      referencedDefinitions().some((item) => item.containerId === id)
    ) {
      record.entity = workspaceContainerRecord(definition);
      await WorkspaceContainerBindings.flush();
      this.engine.rememberOutgoing(record);
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
    parseWorkspaceSyncRecord(record.entity, 2);
    await WorkspaceContainerBindings.flush();
    this.engine.rememberOutgoing(record);
    return record;
  }
  async create(record: FloorpWorkspaceRecord): Promise<void> {
    const expected = recordEpochs.get(record) ??
      this.engine.accountSessionEpoch;
    await ready();
    this.engine.assertAccountSession(expected);
    assertWorkspacesEnabled();
    await waitForProviders(accountAbort.signal);
    await mutate(async () => {
      this.engine.assertAccountSession(expected);
      assertWorkspacesEnabled();
      const entity = parseWorkspaceSyncRecord(record.entity, 2);
      if (entity.kind === "container") {
        if (
          entity.containerId !== record.id || state().records[record.id] ||
          findTab(record.id)
        ) {
          throw new Error("Container record ID collides with another entity");
        }
        if (retired.has(record.id)) {
          throw new Error("Container identity was retired");
        }
        await WorkspaceContainerBindings.receiveDefinitions([entity]);
        this.engine.assertAccountSession(expected);
        for (const id of Object.keys(state().containerPending)) {
          this.engine.incomingWorkspaces?.add(id);
        }
        this.engine._modified.delete(record.id);
        return;
      }
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
      if (WorkspaceContainerBindings.getDefinition(record.id)) {
        throw new Error("Workspace record ID collides with a container");
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
          version: entity.version,
          workspaceId: entity.workspaceId,
          name: entity.name,
          icon: entity.icon,
          defaultContainer: entity.defaultContainer,
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
          version: 2,
          workspaceId: entity.workspaceId,
          name: "",
          icon: null,
          defaultContainer: { kind: "legacy-unresolved" },
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
        await applySelected(
          document,
          expected,
          false,
          this.engine.incomingWorkspaces,
        );
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
    assertWorkspacesEnabled();
    await mutate(() => {
      this.engine.assertAccountSession(expected);
      assertWorkspacesEnabled();
      if (WorkspaceContainerBindings.getDefinition(record.id)) {
        throw new Error(
          "Synced container definitions cannot be removed implicitly",
        );
      }
      delete state().deleted[record.id];
      retire(record.id);
      if (state().records[record.id]) {
        // Workspace deletion disables sync while preserving this device's content.
        delete state().records[record.id];
        delete state().pendingCloses[record.id];
        delete state().pendingBaselines[record.id];
        delete state().containerPending[record.id];
        delete state().protectedTabs[record.id];
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
  incomingWorkspaces: Set<string> | undefined;
  migrationPending = false;
  private accountUid: string | undefined;
  private outgoing = new Map<string, WorkspaceMigrationRecord>();
  // Only successful uploads from this sync count. A restart safely resends all
  // current records while the durable flag remains set.
  private resendAcknowledged = new Map<string, string>();
  constructor(service: unknown) {
    super(WORKSPACE_SYNC_COLLECTION, service);
  }
  async initialize(): Promise<void> {
    await super.initialize();
    // EngineManager awaits initialize before enabled-engine reconciliation.
    // The migration opt-in observer therefore has the durable local state.
    await ready();
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
    // Native enabled-state reconciliation interprets false as an account-wide
    // request to wipe this collection. Feature visibility must not signal that.
    return super.enabled;
  }
  set enabled(value: boolean) {
    super.enabled = value;
  }
  shouldSkipSync(why?: string): boolean {
    return !workspacesEnabled() || super.shouldSkipSync(why);
  }
  pendingMigrationRecords(): WorkspaceMigrationRecord[] {
    this.assertAccountSession();
    return this.migrationPending
      ? WorkspaceContainerMigration.pendingRecords()
      : [];
  }
  rememberOutgoing(record: FloorpWorkspaceRecord): void {
    this.assertAccountSession();
    this.outgoing.set(
      record.id,
      record.deleted
        ? { id: record.id, deleted: true }
        : { id: record.id, entity: structuredClone(record.entity) },
    );
    if (record.deleted && Object.hasOwn(state().deleted, record.id)) {
      sentTombstones.set(record.id, state().deleted[record.id]);
    }
  }
  private currentResendRecords(): WorkspaceMigrationRecord[] {
    const records = new Map<string, WorkspaceMigrationRecord>();
    for (const definition of referencedDefinitions()) {
      records.set(definition.containerId, {
        id: definition.containerId,
        entity: workspaceContainerRecord(definition),
      });
    }
    for (const id of state().selected) {
      const doc = state().records[id];
      if (!doc || captureErrors.has(id)) continue;
      records.set(id, { id, entity: workspaceMetadataRecord(doc) });
      for (const tab of doc.tabs) {
        if (!tab.deleted) {
          records.set(tab.id, {
            id: tab.id,
            entity: workspaceTabRecord(id, tab, doc.version),
          });
        }
      }
    }
    for (const id of Object.keys(state().deleted)) {
      records.set(id, { id, deleted: true });
    }
    return [...records.values()];
  }
  private pendingResendRecords(): WorkspaceMigrationRecord[] {
    return this.currentResendRecords().filter((record) =>
      this.resendAcknowledged.get(record.id) !==
        canonicalWorkspaceMigrationRecord(record)
    );
  }
  private async retrackCurrentRecords(expected: number): Promise<void> {
    this.assertAccountSession(expected);
    const now = Date.now() / 1000;
    for (const { id } of this.pendingResendRecords()) {
      this.assertAccountSession(expected);
      this._modified.set(id, now);
      await this._tracker._saveChangedID(id, now);
    }
  }
  private async settleMigrationResend(expected: number): Promise<void> {
    this.assertAccountSession(expected);
    if (
      !state().migrationResendPending || this.migrationPending ||
      state().selected.some((id) => captureErrors.has(id)) ||
      this.pendingResendRecords().length
    ) return;
    state().migrationResendPending = false;
    try {
      // Persist the exact model whose records were acknowledged before losing
      // the resend obligation. Tracker.saveSoon is not a durable boundary.
      await persistState();
      this.assertAccountSession(expected);
    } catch (error) {
      if (expected === accountEpoch) state().migrationResendPending = true;
      throw error;
    }
  }
  async _processIncoming(): Promise<void> {
    const expected = this.accountSessionEpoch;
    this.assertAccountSession(expected);
    assertWorkspacesEnabled();
    // info/collections was fetched before migration. Another owner's completed
    // migration can make that timestamp stale; bootstrap must perform a GET.
    if (state().bootstrapPending) this.lastModified = null;
    await super._processIncoming();
    this.assertAccountSession(expected);
    // Store reconciliation may remove matching staged IDs from _modified. They
    // still need an acknowledged upload to satisfy this migration manifest.
    const pendingMigration = this.pendingMigrationRecords();
    if (this.migrationPending) {
      const pendingIds = new Set(pendingMigration.map((record) => record.id));
      for (const id of this._modified.ids()) {
        if (!pendingIds.has(id)) this._modified.delete(id);
      }
    } else {
      // Startup collected these definitions before incoming parent tombstones
      // could remove their last selected reference. Skip the obsolete upload;
      // retain the definition locally and on the server for other profiles.
      const referenced = new Set(
        referencedDefinitions().map((definition) => definition.containerId),
      );
      for (const id of this._modified.ids()) {
        if (
          WorkspaceContainerBindings.getDefinition(id) && !referenced.has(id)
        ) {
          this._modified.delete(id);
        }
      }
    }
    for (const record of pendingMigration) {
      this._modified.set(record.id, Date.now() / 1000);
    }
    if (!this.migrationPending && state().migrationResendPending) {
      await this.retrackCurrentRecords(expected);
    }
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
    if (state().bootstrapPending) {
      await mutate(async () => {
        this.assertAccountSession(expected);
        state().bootstrapPending = false;
        await persistState();
        // Each first-pass apply was deferred until all parent tombstones and
        // container definitions were downloaded successfully.
        await retryPendingApplies(expected);
      });
    }
    await this._flushIncomingTabs();
    // An empty current selection has no upload callback. It may discharge the
    // obligation only after a successful incoming pass and capture.
    await mutate(() => this.settleMigrationResend(expected));
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
    const pending = new Set<string>();
    this.incomingWorkspaces = pending;
    let failed: string[];
    try {
      failed = await super._applyRecords(records, telemetry) as string[];
    } finally {
      if (this.incomingWorkspaces === pending) {
        this.incomingWorkspaces = undefined;
      }
    }
    await ready();
    await mutate(async () => {
      this.assertAccountSession(expected);
      captureNow();
      for (const id of pending) {
        if (state().bootstrapPending) continue;
        const document = state().records[id];
        if (
          !document || !state().selected.includes(id) || retired.has(id) ||
          captureErrors.has(id)
        ) continue;
        try {
          await applySelected(document, expected);
        } catch (error) {
          this.assertAccountSession(expected);
          // Base Store catches per-record failures, but deferred UI work runs
          // after it returns. Preserve native retry and our closure fence for
          // every incoming entity whose workspace could not reach the UI.
          for (const record of records) {
            if (
              record.entity?.kind !== "container" &&
              record.entity?.workspaceId === id && !failed.includes(record.id)
            ) {
              failed.push(record.id);
            }
          }
          (telemetry as { addIncomingFailedReason(reason: string): void })
            .addIncomingFailedReason("Workspace UI apply failed");
          console.error(
            "[WorkspacesSync] Deferred workspace apply failed",
            id,
            error,
          );
        }
      }
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
    assertWorkspacesEnabled();
    await waitForProviders(accountAbort.signal);
    await mutate(async () => {
      this.assertAccountSession(expected);
      assertWorkspacesEnabled();
      captureNow();
      for (const [id, tabs] of Object.entries(state().pendingCloses)) {
        const document = state().records[id];
        if (document && !retired.has(id)) {
          if (!state().selected.includes(id) || captureErrors.has(id)) continue;
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
    assertWorkspacesEnabled();
    if (this.migrationPending) {
      return Object.fromEntries(
        this.pendingMigrationRecords().map((
          record,
        ) => [record.id, Date.now() / 1000]),
      );
    }
    const tracked = await this._tracker.getChangedIDs() as Record<
      string,
      number
    >;
    this.assertAccountSession(expected);
    const result = Object.fromEntries(
      Object.entries(tracked).filter(([id]) => {
        if (Object.hasOwn(state().deleted, id)) return true;
        const workspaceId = state().records[id]
          ? id
          : findTab(id)?.document.workspaceId;
        return workspaceId && state().selected.includes(workspaceId) &&
          !captureErrors.has(workspaceId);
      }),
    );
    for (const definition of referencedDefinitions()) {
      result[definition.containerId] = Date.now() / 1000;
    }
    if (state().migrationResendPending) {
      for (const { id } of this.pendingResendRecords()) {
        result[id] = Date.now() / 1000;
      }
    }
    return result;
  }
  async _syncStartup(): Promise<void> {
    this.migrationPending = false;
    this.outgoing.clear();
    this.resendAcknowledged.clear();
    assertWorkspacesEnabled();
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
    assertWorkspacesEnabled();
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
    await WorkspaceContainerBindings.ready(user.uid, deviceId());
    await FloorpWorkspacesSync.capture();
    await trackedChanges;
    this.assertAccountSession(expected);
    const legacy = state().migrationId
      ? null
      : await legacyMigrationInput(user.uid);
    this.assertAccountSession(expected);
    const documents = new Map<string, WorkspaceSyncDocument>();
    for (const document of legacy?.documents ?? []) {
      documents.set(
        document.workspaceId,
        migrateWorkspaceSyncDocumentToV2(document),
      );
    }
    for (const document of Object.values(state().records)) {
      const previous = documents.get(document.workspaceId);
      const current = migrateWorkspaceSyncDocumentToV2(document);
      documents.set(
        document.workspaceId,
        previous ? mergeWorkspaceSyncDocuments(previous, current) : current,
      );
    }
    const prepared = await WorkspaceContainerMigration.prepare(
      this.service as WorkspaceMigrationService,
      {
        accountUid: user.uid,
        deviceId: deviceId(),
        localDocuments: [...documents.values()],
        localRetired: [
          ...new Set([...(legacy?.retired ?? []), ...state().retired]),
        ],
        localMetadataKnown: [
          ...new Set([
            ...(legacy?.metadataKnown ?? []),
            ...state().metadataKnown,
          ]),
        ],
        containerDefinitions: WorkspaceContainerBindings.listDefinitions(),
        assertCurrent: () => {
          this.assertAccountSession(expected);
          assertWorkspacesEnabled();
        },
      },
    );
    this.assertAccountSession(expected);
    if (
      state().migrationResendPending && state().migrationId &&
      state().migrationId !== prepared.migrationId
    ) {
      // An externally replaced migration authority cannot authorize replay of
      // this owner's outstanding model. Preserve it for recovery, without
      // weakening the completed-account cutoff.
      throw new Error("Workspace migration changed with local resend pending");
    }
    this.migrationPending = prepared.phase === "pending";
    await mutate(async () => {
      this.assertAccountSession(expected);
      if (prepared.phase === "pending") state().migrationResendPending = true;
      if (state().migrationId !== prepared.migrationId) {
        // Selection and exclusions come only from this profile's legacy state.
        if (legacy) {
          const explicitSelections = new Set(state().selected);
          state().excluded = [
            ...new Set([
              ...state().excluded,
              ...legacy.excluded.filter((id) => !explicitSelections.has(id)),
            ]),
          ];
          state().selected = [
            ...new Set([...state().selected, ...legacy.selected]),
          ]
            .filter((id) => !state().excluded.includes(id));
        }
        if (prepared.phase === "pending") {
          for (const doc of prepared.snapshot.documents) {
            if (retired.has(doc.workspaceId)) continue;
            const current = state().records[doc.workspaceId];
            // The uploaded migration manifest stays immutable, but local edits
            // made while its network claim was pending must remain authoritative
            // under the normal merge clocks. Completion will retrack them.
            state().records[doc.workspaceId] = current
              ? mergeWorkspaceSyncDocuments(current, doc)
              : doc;
          }
          state().metadataKnown = [
            ...new Set([
              ...state().metadataKnown,
              ...prepared.snapshot.metadataKnown,
            ]),
          ];
          for (const id of prepared.snapshot.retiredIds) {
            retire(id);
            delete state().records[id];
          }
          await WorkspaceContainerBindings.receiveDefinitions(
            prepared.snapshot.definitions,
          );
          for (const id of state().selected) {
            if (state().records[id] && !state().pendingApplies.includes(id)) {
              state().pendingApplies.push(id);
            }
          }
        }
        state().migrationId = prepared.migrationId;
        state().bootstrapPending = true;
        Services.prefs.setStringPref(
          `services.sync.${WORKSPACE_SYNC_COLLECTION}.lastSync`,
          "0",
        );
      }
      // The native startup clears the tracker after pulling the fixed manifest.
      // Keep the current-model resend obligation durable before that happens.
      await persistState();
    });
    // prepare committed a compatible native meta/global engine entry. Calling
    // the base startup before that boundary would invoke its collection wipe.
    await super._syncStartup();
    this.assertAccountSession(expected);
  }
  async _reconcile(record: FloorpWorkspaceRecord): Promise<boolean> {
    const expected = recordEpochs.get(record) ?? this.accountSessionEpoch;
    recordEpochs.set(record, expected);
    await ready();
    this.assertAccountSession(expected);
    assertWorkspacesEnabled();
    if (!isWorkspaceSyncId(record.id)) {
      throw new Error("Invalid incoming Sync record ID");
    }
    if (record.deleted) {
      this._modified.delete(record.id);
      return true;
    }
    const entity = parseWorkspaceSyncRecord(record.entity, 2);
    if (entity.kind === "container") {
      if (
        entity.containerId !== record.id || state().records[record.id] ||
        findTab(record.id) || retired.has(record.id)
      ) {
        throw new Error("Incoming container record ID conflict");
      }
      return true;
    }
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
    this.assertAccountSession(expected);
    const sent = succeeded.flatMap((id) => {
      const record = this.outgoing.get(id);
      return record ? [record] : [];
    });
    const wasMigrating = this.migrationPending;
    if (this.migrationPending) {
      await mutate(async () => {
        this.assertAccountSession(expected);
        state().migrationResendPending = true;
        // Capture may have persisted newer model data after staging. Save it
        // before acknowledge can durably publish the migration complete marker.
        await persistState();
      });
      const result = await WorkspaceContainerMigration.acknowledge(
        this.service as WorkspaceMigrationService,
        sent,
        () => {
          this.assertAccountSession(expected);
          assertWorkspacesEnabled();
        },
      );
      this.assertAccountSession(expected);
      if (result.phase === "complete") {
        this.migrationPending = false;
        // Initial staged records are immutable. Preserve edits accumulated
        // while they were in flight by tracking the current model afterwards.
      }
    }
    await mutate(async () => {
      this.assertAccountSession(expected);
      if (!wasMigrating && state().migrationResendPending) {
        for (const record of sent) {
          this.resendAcknowledged.set(
            record.id,
            canonicalWorkspaceMigrationRecord(record),
          );
        }
      }
      for (const id of succeeded) {
        this.outgoing.delete(id);
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
      await this.settleMigrationResend(expected);
    });
    if (!this.migrationPending && state().migrationResendPending) {
      await this.retrackCurrentRecords(expected);
    }
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
