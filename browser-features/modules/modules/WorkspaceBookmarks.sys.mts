// SPDX-License-Identifier: MPL-2.0

/**
 * A profile-local view of Places. The original bookmarks and their Sync data
 * remain owned by Places; these rules only select which items browser UI uses.
 */
export const WORKSPACE_BOOKMARKS_ENABLED_PREF =
  "floorp.workspaces.bookmarks.enabled";
export const WORKSPACE_BOOKMARKS_CHANGED_TOPIC =
  "floorp.workspace-bookmarks.changed";
const INITIALIZED_PREF = "floorp.workspaces.bookmarks.initialized";
const WORKSPACES_ENABLED_PREF = "floorp.workspaces.enabled";
const WORKSPACE_STORE_PREF = "floorp.workspaces.v4.store";
const GUID = /^[A-Za-z0-9_-]{12}$/;
const WORKSPACE_ID =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const ROOT_GUIDS = new Set([
  "root________",
  "toolbar_____",
  "menu________",
  "unfiled_____",
  "mobile______",
  "tags________",
]);

export interface WorkspaceBookmarkItem {
  guid: string;
  parentGuid: string | null;
  title: string;
  url: string | null;
  type: number;
  id?: number;
}

export interface WorkspaceBookmarkTree {
  guid: string;
  parentGuid?: string;
  title?: string;
  uri?: string;
  url?: string;
  typeCode?: number;
  type?: number | string;
  id?: number;
  children?: WorkspaceBookmarkTree[];
}

export interface WorkspaceBookmarkContext {
  readonly workspaceId: string | null;
  readonly enabled: boolean;
  readonly scopeEnabled: boolean;
  readonly ready: boolean;
  readonly generation: number;
  readonly windowGeneration: number;
  readonly windowId: number | null;
  readonly allItems: boolean;
}

export interface WorkspaceBookmarkState {
  schemaVersion: 1;
  sourceId: string;
  rules: Record<string, string[]>;
  workspaceFolders: Record<string, string>;
  /** A crash after native copying must never publish unassigned copies. */
  pendingCopy?: { knownFolders: string[] };
}

export interface WorkspaceBookmarkBackup extends WorkspaceBookmarkState {
  kind: "floorp-workspace-bookmark-scopes";
  createdAt: number;
  workspaceNames: Record<string, string>;
}

export interface WorkspaceBookmarkImportMapping {
  folders?: Readonly<Record<string, string>>;
  workspaces?: Readonly<Record<string, string>>;
}

export interface WorkspaceBookmarkImportPreview {
  generation: number;
  knownWorkspaceIds: readonly string[];
  settings: WorkspaceBookmarkBackup;
  applicableRules: Array<{ guid: string; workspaceIds: string[] }>;
  unresolvedFolders: string[];
  unresolvedWorkspaces: string[];
  changes: Array<{
    guid: string;
    title: string;
    before: string[] | null;
    after: string[];
  }>;
  sourceMatches: boolean;
}

export interface WorkspaceBookmarkEffectiveScope {
  workspaceIds: readonly string[] | null;
  inheritedFrom: readonly string[];
}

export interface WorkspaceBookmarkTransferPlan {
  context: WorkspaceBookmarkContext;
  sourceGuids: string[];
  targetParentGuid: string;
  doCopy: boolean;
  requiresConfirmation: boolean;
  hiddenDescendantCount: number;
  explicitRules: Record<string, string[]>;
  sourceTrees: Record<string, WorkspaceBookmarkTree>;
}

interface BookmarkStorage {
  read(): Promise<unknown | null>;
  write(
    value: WorkspaceBookmarkState,
    preserveLastGood: boolean,
  ): Promise<void>;
  readRecovery(): Promise<unknown | null>;
}

interface BookmarkPlaces {
  history?: { makeGuid(): string };
  bookmarks: {
    rootGuid: string;
    unfiledGuid: string;
    TYPE_BOOKMARK: number;
    TYPE_FOLDER: number;
    fetch(
      input: string | { guid: string },
    ): Promise<WorkspaceBookmarkItem | null>;
    insert(input: {
      guid?: string;
      parentGuid: string;
      type: number;
      title: string;
    }): Promise<WorkspaceBookmarkItem>;
    remove(
      input: string,
      options?: { preventRemovalOfNonEmptyFolders?: boolean },
    ): Promise<void>;
  };
  promiseBookmarksTree(
    guid: string,
    options?: { includeItemIds?: boolean },
  ): Promise<WorkspaceBookmarkTree | null>;
}

interface BookmarkChanges {
  addListener(
    events: string[],
    listener: (changes: BookmarkChange[]) => void,
  ): void;
  removeListener(
    events: string[],
    listener: (changes: BookmarkChange[]) => void,
  ): void;
}

interface BookmarkChange {
  type: string;
  id?: number;
  guid?: string;
  oldGuid?: string;
}

interface ServiceOptions {
  storage?: BookmarkStorage;
  places?: BookmarkPlaces;
  observe?: boolean;
  changes?: BookmarkChanges;
  sourceId?: string;
  readEnabled?: () => boolean;
  writeEnabled?: (enabled: boolean) => void;
  knownWorkspaces?: () => string[];
}

interface WindowContext {
  id: number;
  workspaceId: string | null;
  enabled: boolean;
  generation: number;
}

const CHANGE_EVENTS = [
  "bookmark-added",
  "bookmark-removed",
  "bookmark-moved",
  "bookmark-guid-changed",
  "bookmark-title-changed",
  "bookmark-url-changed",
  "bookmark-tags-changed",
  "bookmark-keyword-changed",
  "purge-caches",
];

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function workspaceIds(value: unknown): string[] {
  if (
    !Array.isArray(value) ||
    value.some((id) => typeof id !== "string" || !WORKSPACE_ID.test(id))
  ) {
    throw new TypeError("Invalid workspace bookmark scope");
  }
  return [...new Set(value as string[])].sort();
}

/** Invalid input is never treated as an empty, globally visible rule set. */
export function parseWorkspaceBookmarkState(
  value: unknown,
): WorkspaceBookmarkState {
  if (
    !isRecord(value) || value.schemaVersion !== 1 ||
    typeof value.sourceId !== "string" || !WORKSPACE_ID.test(value.sourceId) ||
    !isRecord(value.rules) || !isRecord(value.workspaceFolders)
  ) {
    throw new TypeError("Invalid workspace bookmark settings");
  }
  const rules: Record<string, string[]> = Object.create(null);
  const folders: Record<string, string> = Object.create(null);
  let pendingCopy: WorkspaceBookmarkState["pendingCopy"];
  if (value.pendingCopy !== undefined) {
    if (
      !isRecord(value.pendingCopy) ||
      !Array.isArray(value.pendingCopy.knownFolders) ||
      value.pendingCopy.knownFolders.some((guid) =>
        typeof guid !== "string" || !GUID.test(guid)
      )
    ) {
      throw new TypeError("Invalid pending bookmark copy");
    }
    pendingCopy = {
      knownFolders: [...new Set(value.pendingCopy.knownFolders as string[])]
        .sort(),
    };
  }
  for (const [guid, ids] of Object.entries(value.rules)) {
    if (!GUID.test(guid) || ROOT_GUIDS.has(guid)) {
      throw new TypeError("Invalid scoped bookmark folder GUID");
    }
    rules[guid] = workspaceIds(ids);
  }
  for (const [id, guid] of Object.entries(value.workspaceFolders)) {
    if (
      !WORKSPACE_ID.test(id) || typeof guid !== "string" || !GUID.test(guid) ||
      ROOT_GUIDS.has(guid)
    ) {
      throw new TypeError("Invalid workspace bookmark save folder");
    }
    folders[id] = guid;
  }
  return {
    schemaVersion: 1,
    sourceId: value.sourceId,
    rules,
    workspaceFolders: folders,
    ...(pendingCopy ? { pendingCopy } : {}),
  };
}

function parseBackup(raw: unknown): WorkspaceBookmarkBackup {
  const value: unknown = typeof raw === "string" ? JSON.parse(raw) : raw;
  if (
    !isRecord(value) || value.kind !== "floorp-workspace-bookmark-scopes" ||
    typeof value.createdAt !== "number" || !Number.isFinite(value.createdAt) ||
    !isRecord(value.workspaceNames) ||
    Object.entries(value.workspaceNames).some(([id, name]) =>
      !WORKSPACE_ID.test(id) || typeof name !== "string"
    )
  ) {
    throw new TypeError("Invalid workspace bookmark backup");
  }
  return {
    ...parseWorkspaceBookmarkState(value),
    kind: "floorp-workspace-bookmark-scopes",
    createdAt: value.createdAt,
    workspaceNames: { ...value.workspaceNames } as Record<string, string>,
  };
}

function newState(sourceId?: string): WorkspaceBookmarkState {
  return {
    schemaVersion: 1,
    sourceId: sourceId ??
      Services.uuid.generateUUID().toString().replace(/[{}]/g, ""),
    rules: Object.create(null),
    workspaceFolders: Object.create(null),
  };
}

function stateFingerprint(value: WorkspaceBookmarkState): string {
  return JSON.stringify({
    sourceId: value.sourceId,
    rules: Object.entries(value.rules).sort(([left], [right]) =>
      left.localeCompare(right)
    ),
    workspaceFolders: Object.entries(value.workspaceFolders).sort((
      [left],
      [right],
    ) => left.localeCompare(right)),
    pendingCopy: value.pendingCopy ?? null,
  });
}

function defaultPlaces(): BookmarkPlaces {
  return (ChromeUtils.importESModule(
    "resource://gre/modules/PlacesUtils.sys.mjs",
  ) as { PlacesUtils: BookmarkPlaces }).PlacesUtils;
}

function defaultStorage(): BookmarkStorage {
  const directory = PathUtils.join(PathUtils.profileDir, "workspaces");
  const filename = PathUtils.join(directory, "bookmark-scopes.json");
  const lastGood = `${filename}.last-good`;
  return {
    async read() {
      if (!await IOUtils.exists(filename)) {
        if (Services.prefs.getBoolPref(INITIALIZED_PREF, false)) {
          throw new Error("Workspace bookmark settings are missing");
        }
        return null;
      }
      return await IOUtils.readJSON(filename);
    },
    async write(value, preserveLastGood) {
      await IOUtils.makeDirectory(directory, {
        createAncestors: true,
        ignoreExisting: true,
        permissions: 0o700,
      });
      await IOUtils.writeJSON(filename, value, {
        tmpPath: `${filename}.tmp`,
        ...(preserveLastGood && await IOUtils.exists(filename)
          ? { backupFile: lastGood }
          : {}),
        flush: true,
      });
      Services.prefs.setBoolPref(INITIALIZED_PREF, true);
    },
    async readRecovery() {
      if (!await IOUtils.exists(lastGood)) return null;
      return await IOUtils.readJSON(lastGood);
    },
  };
}

function defaultKnownWorkspaces(): string[] {
  try {
    const value: unknown = JSON.parse(
      Services.prefs.getStringPref(WORKSPACE_STORE_PREF, "{}"),
    );
    if (!isRecord(value)) return [];
    const data = value.data;
    return Array.isArray(data)
      ? data.filter((entry) =>
        Array.isArray(entry) && typeof entry[0] === "string"
      )
        .map((entry) => entry[0] as string)
      : isRecord(data)
      ? Object.keys(data)
      : [];
  } catch {
    return [];
  }
}

function ownerWindow(window: Window | null): Window | null {
  if (!window) return null;
  try {
    const owner = (window as unknown as {
      browsingContext?: { topChromeWindow?: Window | null };
    }).browsingContext?.topChromeWindow ?? window;
    if (
      owner.document?.documentURI ===
        "chrome://browser/content/places/bookmarkProperties.xhtml" &&
      owner.opener
    ) return ownerWindow(owner.opener as Window);
    return owner;
  } catch {
    return window;
  }
}

function isPrivateWindow(window: Window | null): boolean {
  if (!window) return false;
  try {
    const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
    ) as { PrivateBrowsingUtils: { isWindowPrivate(window: Window): boolean } };
    return PrivateBrowsingUtils.isWindowPrivate(window);
  } catch {
    return false;
  }
}

export class WorkspaceBookmarksService {
  private readonly storage: BookmarkStorage;
  private readonly places: BookmarkPlaces;
  private readonly readEnabled: () => boolean;
  private readonly writeEnabled: (enabled: boolean) => void;
  private readonly knownWorkspaces: () => string[];
  private state: WorkspaceBookmarkState;
  private storeError: string | null = null;
  private indexError: string | null = null;
  private indexReady = false;
  private indexRevision = 0;
  private pendingIdentityChanges = 0;
  private unresolvedInitialIdentityChange = false;
  private initialized = false;
  private recoveryAvailable = false;
  private generation = 0;
  private readonly items = new Map<string, WorkspaceBookmarkItem>();
  private readonly children = new Map<string, string[]>();
  private readonly byUrl = new Map<string, string[]>();
  private readonly byItemId = new Map<number, string>();
  private readonly windows = new WeakMap<Window, WindowContext>();
  private readonly windowsById = new Map<number, WeakRef<Window>>();
  private readonly rememberedFolders = new Map<string, string>();
  private nextWindowId = 0;
  private readonly initialization: Promise<void>;
  private refreshTask: Promise<void> | null = null;
  private writeTask: Promise<void> = Promise.resolve();
  private observeChanges: BookmarkChanges | null = null;
  private visibleCache = new Map<string, readonly string[]>();
  private visibleJSONCache = new Map<string, string>();
  private effectiveCache = new Map<string, WorkspaceBookmarkEffectiveScope>();
  private readonly activeTransfers = new Set<WorkspaceBookmarkTransferPlan>();
  private readonly changeListener = (changes: BookmarkChange[]) => {
    for (const change of changes) {
      // Places emits the new GUID plus its stable item ID, not the old GUID.
      const previousGuid = change.oldGuid ??
        (change.id === undefined ? undefined : this.byItemId.get(change.id));
      if (change.type === "bookmark-guid-changed" && !previousGuid) {
        if (!this.initialized) this.unresolvedInitialIdentityChange = true;
        if (Object.keys(this.state.rules).length > 0) {
          this.failStore(
            new Error(
              "A bookmark identity changed before it could be resolved",
            ),
          );
        }
      }
      if (
        change.type === "bookmark-guid-changed" && previousGuid &&
        change.guid &&
        previousGuid !== change.guid
      ) {
        const oldGuid = previousGuid;
        const newGuid = change.guid;
        this.pendingIdentityChanges++;
        void this.mutate((next) => {
          if (next.rules[oldGuid]) next.rules[newGuid] = next.rules[oldGuid];
          delete next.rules[oldGuid];
          for (const id of Object.keys(next.workspaceFolders)) {
            if (next.workspaceFolders[id] === oldGuid) {
              next.workspaceFolders[id] = newGuid;
            }
          }
        }).catch((error) => this.failStore(error)).finally(() => {
          this.pendingIdentityChanges--;
          this.changed();
        });
      }
      if (change.id !== undefined) {
        if (change.type === "bookmark-removed") this.byItemId.delete(change.id);
        else if (
          change.guid &&
          (change.type === "bookmark-added" ||
            change.type === "bookmark-guid-changed")
        ) {
          this.byItemId.set(change.id, change.guid);
        }
      }
    }
    // Removed GUID rules are intentionally retained for Undo and restoration.
    this.indexReady = false;
    this.changed();
    void this.refresh().catch((error) => {
      this.indexError = String(error);
      this.changed();
    });
  };

  constructor(options: ServiceOptions = {}) {
    this.storage = options.storage ?? defaultStorage();
    this.places = options.places ?? defaultPlaces();
    this.readEnabled = options.readEnabled ??
      (() =>
        Services.prefs.getBoolPref(WORKSPACE_BOOKMARKS_ENABLED_PREF, true) &&
        Services.prefs.getBoolPref(WORKSPACES_ENABLED_PREF, true));
    this.writeEnabled = options.writeEnabled ??
      ((enabled) =>
        Services.prefs.setBoolPref(WORKSPACE_BOOKMARKS_ENABLED_PREF, enabled));
    this.knownWorkspaces = options.knownWorkspaces ?? defaultKnownWorkspaces;
    this.state = newState(options.sourceId);
    this.initialization = this.initialize();
    if (options.observe !== false) {
      this.observeChanges = options.changes ?? (ChromeUtils.importESModule(
        "resource://gre/modules/PlacesUtils.sys.mjs",
      ) as { PlacesUtils: { observers: BookmarkChanges } }).PlacesUtils
        .observers;
      this.observeChanges.addListener(CHANGE_EVENTS, this.changeListener);
      Services.prefs.addObserver(WORKSPACE_BOOKMARKS_ENABLED_PREF, this);
      Services.prefs.addObserver(WORKSPACES_ENABLED_PREF, this);
      Services.prefs.addObserver(WORKSPACE_STORE_PREF, this);
    }
  }

  get ready(): Promise<void> {
    return this.initialization.then(() =>
      this.refreshTask ?? Promise.resolve()
    );
  }

  observe(): void {
    this.changed();
  }

  destroy(): void {
    this.observeChanges?.removeListener(CHANGE_EVENTS, this.changeListener);
    if (this.observeChanges) {
      Services.prefs.removeObserver(WORKSPACE_BOOKMARKS_ENABLED_PREF, this);
      Services.prefs.removeObserver(WORKSPACES_ENABLED_PREF, this);
      Services.prefs.removeObserver(WORKSPACE_STORE_PREF, this);
    }
    this.observeChanges = null;
    this.windowsById.clear();
  }

  private async initialize(): Promise<void> {
    const results = await Promise.allSettled([
      this.storage.read(),
      this.readIndex(),
      this.storage.readRecovery(),
    ]);
    const stored = results[0];
    try {
      if (stored.status === "rejected") throw stored.reason;
      if (stored.value !== null) {
        this.state = parseWorkspaceBookmarkState(stored.value);
      }
      if (this.state.pendingCopy) {
        throw new Error(
          "A bookmark copy was interrupted before its workspace rules were saved. Review all bookmarks and recover the display settings.",
        );
      }
      if (
        this.unresolvedInitialIdentityChange &&
        Object.keys(this.state.rules).length > 0
      ) {
        throw new Error(
          "A bookmark identity changed before settings finished loading",
        );
      }
    } catch (error) {
      this.failStore(error);
    }
    if (results[1].status === "rejected") {
      this.indexError = String(results[1].reason);
    }
    this.recoveryAvailable = results[2].status === "fulfilled" &&
      results[2].value !== null;
    this.initialized = true;
    this.changed();
  }

  private failStore(error: unknown): void {
    this.storeError = error instanceof Error ? error.message : String(error);
    console.error("[WorkspaceBookmarks] Settings are unavailable", error);
    this.changed();
  }

  private changed(): void {
    this.generation++;
    this.visibleCache.clear();
    this.visibleJSONCache.clear();
    this.effectiveCache.clear();
    Services.obs.notifyObservers(
      null as unknown as nsISupports,
      WORKSPACE_BOOKMARKS_CHANGED_TOPIC,
      String(this.generation),
    );
  }

  private async readIndex(): Promise<void> {
    const revision = this.indexRevision;
    const tree = await this.places.promiseBookmarksTree(
      this.places.bookmarks.rootGuid,
      {
        includeItemIds: true,
      },
    );
    // A change during an outstanding fetch needs another snapshot. Never
    // publish a tree that predates an already observed Places notification.
    if (revision !== this.indexRevision) return this.readIndex();
    if (!tree) throw new Error("Places returned no bookmark tree");
    const items = new Map<string, WorkspaceBookmarkItem>();
    const children = new Map<string, string[]>();
    const byUrl = new Map<string, string[]>();
    const byItemId = new Map<number, string>();
    const visit = (
      node: WorkspaceBookmarkTree,
      parentGuid: string | null,
    ): void => {
      const type = node.typeCode ??
        (typeof node.type === "number"
          ? node.type
          : node.type === "text/x-moz-place-container" || node.children
          ? 2
          : node.type === "text/x-moz-place-separator"
          ? 3
          : 1);
      const item: WorkspaceBookmarkItem = {
        guid: node.guid,
        parentGuid: parentGuid ?? node.parentGuid ?? null,
        title: node.title ?? "",
        url: node.uri ?? node.url ?? null,
        type,
        id: node.id,
      };
      items.set(item.guid, item);
      children.set(item.guid, (node.children ?? []).map((child) => child.guid));
      if (item.id !== undefined) byItemId.set(item.id, item.guid);
      if (item.url && type === 1) {
        const guids = byUrl.get(item.url) ?? [];
        guids.push(item.guid);
        byUrl.set(item.url, guids);
      }
      for (const child of node.children ?? []) visit(child, item.guid);
    };
    visit(tree, null);
    this.items.clear();
    this.children.clear();
    this.byUrl.clear();
    this.byItemId.clear();
    for (const [key, value] of items) this.items.set(key, value);
    for (const [key, value] of children) this.children.set(key, value);
    for (const [key, value] of byUrl) this.byUrl.set(key, value);
    for (const [key, value] of byItemId) this.byItemId.set(key, value);
    this.indexError = null;
    this.indexReady = true;
  }

  async refresh(): Promise<void> {
    this.indexRevision++;
    this.indexReady = false;
    await this.initialization;
    if (this.refreshTask) return this.refreshTask;
    this.refreshTask = this.readIndex().then(() => {
      this.changed();
    }).catch((error) => {
      this.indexError = String(error);
      this.changed();
      throw error;
    }).finally(() => {
      this.refreshTask = null;
    });
    return this.refreshTask;
  }

  getEnabled(): boolean {
    return this.readEnabled();
  }

  setEnabled(enabled: boolean): void {
    this.writeEnabled(Boolean(enabled));
    this.changed();
  }

  getStatus() {
    return {
      state: this.storeError || this.indexError
        ? "error"
        : !this.initialized || !this.indexReady
        ? "loading"
        : "ready",
      enabled: this.getEnabled(),
      error: this.storeError ?? this.indexError,
      hasRules: Object.keys(this.state.rules).length > 0,
      hasRecovery: this.recoveryAvailable,
      generation: this.generation,
    } as const;
  }

  setWindowContext(
    window: Window,
    workspaceId: string | null,
    enabled: boolean,
  ): void {
    window = ownerWindow(window) ?? window;
    if (workspaceId !== null && !WORKSPACE_ID.test(workspaceId)) {
      throw new TypeError("Invalid workspace ID");
    }
    const previous = this.windows.get(window);
    if (
      previous && previous.workspaceId === workspaceId &&
      previous.enabled === enabled
    ) return;
    const next: WindowContext = {
      id: previous?.id ?? ++this.nextWindowId,
      workspaceId,
      enabled,
      generation: (previous?.generation ?? 0) + 1,
    };
    this.windows.set(window, next);
    this.windowsById.set(next.id, new WeakRef(window));
    Services.obs.notifyObservers(
      null as unknown as nsISupports,
      WORKSPACE_BOOKMARKS_CHANGED_TOPIC,
      String(this.generation),
    );
  }

  clearWindowContext(window: Window): void {
    window = ownerWindow(window) ?? window;
    const existing = this.windows.get(window);
    if (existing) this.windowsById.delete(existing.id);
    this.windows.delete(window);
  }

  getContext(
    window: Window | null,
    options: { allItems?: boolean } = {},
  ): WorkspaceBookmarkContext {
    window = ownerWindow(window);
    let registered = window ? this.windows.get(window) : undefined;
    if (window && !registered) {
      registered = {
        id: ++this.nextWindowId,
        workspaceId: null,
        enabled: true,
        generation: 0,
      };
      this.windows.set(window, registered);
      this.windowsById.set(registered.id, new WeakRef(window));
    }
    const allItems = Boolean(
      options.allItems ||
        window?.document?.documentElement?.getAttribute("windowtype") ===
          "Places:Organizer",
    );
    return Object.freeze({
      workspaceId: isPrivateWindow(window)
        ? null
        : registered?.workspaceId ?? null,
      enabled: (registered?.enabled ?? true) && this.getEnabled(),
      scopeEnabled: (registered?.enabled ?? true) && this.getEnabled(),
      ready: this.initialized && this.indexReady && !this.storeError &&
        !this.indexError && this.activeTransfers.size === 0 &&
        this.pendingIdentityChanges === 0 && !this.state.pendingCopy,
      generation: this.generation,
      windowGeneration: registered?.generation ?? 0,
      windowId: registered?.id ?? null,
      allItems,
    });
  }

  isCurrentContext(
    context: WorkspaceBookmarkContext,
    window?: Window | null,
  ): boolean {
    if (context.generation !== this.generation) return false;
    const target = ownerWindow(window ?? null) ??
      (context.windowId !== null
        ? this.windowsById.get(context.windowId)?.deref()
        : null);
    if (context.windowId !== null && (!target || target.closed)) return false;
    const current = this.getContext(target ?? null, {
      allItems: context.allItems,
    });
    return current.windowId === context.windowId &&
      current.workspaceId === context.workspaceId &&
      current.windowGeneration === context.windowGeneration &&
      current.enabled === context.enabled &&
      current.scopeEnabled === context.scopeEnabled &&
      current.ready === context.ready;
  }

  private context(
    input: Window | WorkspaceBookmarkContext | null,
  ): WorkspaceBookmarkContext {
    return input && "scopeEnabled" in input
      ? input as WorkspaceBookmarkContext
      : this.getContext(input as Window | null);
  }

  getScope(guid: string): readonly string[] | null {
    return this.state.rules[guid] ? [...this.state.rules[guid]] : null;
  }

  getEffectiveScope(guid: string): WorkspaceBookmarkEffectiveScope {
    const cached = this.effectiveCache.get(guid);
    if (cached) return cached;
    const inheritedFrom: string[] = [];
    let allowed: Set<string> | null = null;
    const visited = new Set<string>();
    let current: string | null = guid;
    while (current && !visited.has(current)) {
      visited.add(current);
      const rule = this.state.rules[current];
      if (rule) {
        inheritedFrom.push(current);
        allowed = allowed === null
          ? new Set(rule)
          : new Set([...allowed].filter((id: string) => rule.includes(id)));
      }
      current = this.items.get(current)?.parentGuid ?? null;
    }
    const result = Object.freeze({
      workspaceIds: allowed === null
        ? null
        : Object.freeze([...allowed].sort()),
      inheritedFrom: Object.freeze(inheritedFrom),
    });
    this.effectiveCache.set(guid, result);
    return result;
  }

  isVisible(
    guid: string,
    input: Window | WorkspaceBookmarkContext | null,
  ): boolean {
    const context = this.context(input);
    if (!this.isCurrentContext(context)) return false;
    if (context.allItems || !context.enabled) return true;
    if (
      !context.ready || !this.items.has(guid)
    ) return false;
    return this.allowedInContext(guid, context);
  }

  private allowedInContext(
    guid: string,
    context: WorkspaceBookmarkContext,
  ): boolean {
    const scope = this.getEffectiveScope(guid).workspaceIds;
    return scope === null ||
      (context.workspaceId !== null && scope.includes(context.workspaceId));
  }

  isNodeVisible(
    node: { bookmarkGuid?: string; itemId?: number },
    input: Window | WorkspaceBookmarkContext | null,
  ): boolean {
    const guid = node.bookmarkGuid ||
      (node.itemId !== undefined ? this.byItemId.get(node.itemId) : null);
    return guid ? this.isVisible(guid, input) : true;
  }

  isUrlVisible(
    url: string | { href?: string; spec?: string },
    input: Window | WorkspaceBookmarkContext | null,
  ): boolean {
    const value = typeof url === "string" ? url : url.href ?? url.spec ?? "";
    return (this.byUrl.get(value) ?? []).some((guid) =>
      this.isVisible(guid, input)
    );
  }

  async visibleBookmarksForUrl(
    url: string | { href?: string; spec?: string },
    input: Window | WorkspaceBookmarkContext | null,
  ): Promise<WorkspaceBookmarkItem[]> {
    await this.ready;
    const value = typeof url === "string" ? url : url.href ?? url.spec ?? "";
    return (this.byUrl.get(value) ?? []).filter((guid) =>
      this.isVisible(guid, input)
    )
      .map((guid) => ({ ...this.items.get(guid)! }));
  }

  filterItems<T extends { guid?: string; bookmarkGuid?: string }>(
    items: readonly T[],
    input: Window | WorkspaceBookmarkContext | null,
  ): T[] {
    return items.filter((item) => {
      const guid = item.guid ?? item.bookmarkGuid;
      return Boolean(guid && this.isVisible(guid, input));
    });
  }

  getVisibleBookmarkGuids(
    input: Window | WorkspaceBookmarkContext | null,
  ): readonly string[] | null {
    const context = this.context(input);
    if (!this.isCurrentContext(context)) return [];
    if (context.allItems || !context.enabled) return null;
    if (!context.ready) return [];
    if (Object.keys(this.state.rules).length === 0) return null;
    const key = `${context.generation}:${context.workspaceId ?? "common"}`;
    let guids = this.visibleCache.get(key);
    if (!guids) {
      guids = [...this.items.values()].filter((item) =>
        item.type === 1 && this.allowedInContext(item.guid, context)
      ).map((item) => item.guid);
      this.visibleCache.set(key, guids);
    }
    return guids;
  }

  getVisibleBookmarkGuidsJSON(
    input: Window | WorkspaceBookmarkContext | null,
  ): string | null {
    const context = this.context(input);
    const guids = this.getVisibleBookmarkGuids(context);
    if (guids === null) return null;
    if (!context.ready || !this.isCurrentContext(context)) return "[]";
    const key = `${context.generation}:${context.workspaceId ?? "common"}`;
    let json = this.visibleJSONCache.get(key);
    if (json === undefined) {
      json = JSON.stringify(guids);
      this.visibleJSONCache.set(key, json);
    }
    return json;
  }

  private mutate(
    change: (next: WorkspaceBookmarkState) => void,
    recover = false,
  ): Promise<void> {
    const operation = this.writeTask.then(async () => {
      await this.ready;
      if (this.storeError && !recover) throw new Error(this.storeError);
      const next = parseWorkspaceBookmarkState(this.state);
      change(next);
      const checked = parseWorkspaceBookmarkState(next);
      if (!recover && JSON.stringify(checked) === JSON.stringify(this.state)) {
        return;
      }
      await this.storage.write(
        checked,
        !this.storeError && this.initialized && !this.state.pendingCopy,
      );
      this.state = checked;
      this.storeError = null;
      this.recoveryAvailable = true;
      this.changed();
    });
    this.writeTask = operation.catch(() => {});
    return operation;
  }

  async setFolderScope(
    guid: string,
    ids: readonly string[] | null,
  ): Promise<void> {
    await this.ready;
    const folder = this.items.get(guid);
    if (!folder || folder.type !== 2 || ROOT_GUIDS.has(guid)) {
      throw new TypeError("Choose a regular bookmark folder");
    }
    const nextIds = ids === null ? null : workspaceIds(ids);
    await this.mutate((next) => {
      if (nextIds === null) delete next.rules[guid];
      else next.rules[guid] = nextIds;
    });
  }

  async listFolders() {
    await this.ready;
    const path = (guid: string): string => {
      const parts: string[] = [];
      const visited = new Set<string>();
      let current: string | null = guid;
      while (current && !visited.has(current)) {
        visited.add(current);
        const item = this.items.get(current);
        if (!item) break;
        if (item.title) parts.unshift(item.title);
        current = item.parentGuid;
      }
      return parts.join(" / ");
    };
    return [...this.items.values()].filter((item) =>
      item.type === 2 && !ROOT_GUIDS.has(item.guid)
    )
      .map((item) => ({
        guid: item.guid,
        title: item.title,
        path: path(item.guid),
        scope: this.getScope(item.guid),
        effectiveScope: this.getEffectiveScope(item.guid),
      }));
  }

  async reassignWorkspace(oldId: string, newId: string): Promise<void> {
    if (!WORKSPACE_ID.test(oldId) || !WORKSPACE_ID.test(newId)) {
      throw new TypeError("Invalid workspace remapping");
    }
    await this.mutate((next) => {
      for (const guid of Object.keys(next.rules)) {
        next.rules[guid] = [
          ...new Set(next.rules[guid].map((id) => id === oldId ? newId : id)),
        ].sort();
      }
      if (next.workspaceFolders[oldId]) {
        next.workspaceFolders[newId] = next.workspaceFolders[oldId];
        delete next.workspaceFolders[oldId];
      }
    });
  }

  async getDefaultFolder(
    window: Window,
    preferredGuid?: string,
  ): Promise<string> {
    await this.ready;
    const context = this.getContext(window);
    if (context.enabled && !context.allItems && !context.ready) {
      throw new Error("Workspace bookmark settings are unavailable");
    }
    const remembered = this.rememberedFolders.get(
      `${context.windowId}:${context.workspaceId}`,
    );
    for (const candidate of [remembered, preferredGuid]) {
      const folder = candidate ? this.items.get(candidate) : undefined;
      if (folder?.type === 2 && this.isVisible(folder.guid, context)) {
        return folder.guid;
      }
    }
    return this.places.bookmarks.unfiledGuid;
  }

  rememberDefaultFolder(window: Window, guid: string): void {
    const context = this.getContext(window);
    if (this.items.get(guid)?.type === 2 && this.isVisible(guid, context)) {
      this.rememberedFolders.set(
        `${context.windowId}:${context.workspaceId}`,
        guid,
      );
    }
  }

  async getWorkspaceFolder(window: Window, title: string): Promise<string> {
    return (await this.getOrCreateWorkspaceFolder(window, title)).guid;
  }

  async getOrCreateWorkspaceFolder(
    window: Window,
    title?: string,
  ): Promise<{ guid: string; created: boolean; workspaceId: string }> {
    await this.ready;
    const context = this.getContext(window);
    if (!context.workspaceId || !context.enabled || !context.ready) {
      throw new Error("No active workspace");
    }
    const existing = this.state.workspaceFolders[context.workspaceId];
    const existingScope = existing
      ? this.getEffectiveScope(existing).workspaceIds
      : null;
    if (
      existing && this.items.get(existing)?.type === 2 &&
      existingScope?.length === 1 && existingScope[0] === context.workspaceId
    ) {
      return {
        guid: existing,
        created: false,
        workspaceId: context.workspaceId,
      };
    }
    const guid = this.places.history?.makeGuid();
    if (!guid || !GUID.test(guid)) {
      throw new Error("Could not allocate a bookmark folder GUID");
    }
    const workspaceId = context.workspaceId;
    let folderCreated = false;
    try {
      // Reserve its rule before Places can notify views about the new GUID.
      await this.mutate((next) => {
        next.rules[guid] = [workspaceId];
        next.workspaceFolders[workspaceId] = guid;
      });
      const current = this.getContext(window);
      if (
        current.workspaceId !== workspaceId || !current.enabled ||
        current.windowId !== context.windowId ||
        current.windowGeneration !== context.windowGeneration ||
        current.scopeEnabled !== context.scopeEnabled || window.closed
      ) {
        throw new Error("The workspace changed before saving the bookmark");
      }
      let workspaceName = workspaceId;
      if (!title) {
        try {
          const data = JSON.parse(
            Services.prefs.getStringPref(WORKSPACE_STORE_PREF, "{}"),
          );
          const entry: unknown = Array.isArray(data.data)
            ? data.data.find((item: unknown) =>
              Array.isArray(item) && item[0] === workspaceId
            )?.[1]
            : data.data?.[workspaceId];
          if (isRecord(entry) && typeof entry.name === "string") {
            workspaceName = entry.name;
          }
        } catch {
          // Its stable ID is still a usable, non-inferred title fallback.
        }
      }
      await this.places.bookmarks.insert({
        guid,
        parentGuid: this.places.bookmarks.unfiledGuid,
        type: this.places.bookmarks.TYPE_FOLDER,
        title: title ?? workspaceName,
      });
      folderCreated = true;
      await this.refresh();
      return { guid, created: true, workspaceId };
    } catch (error) {
      let folderRemoved = !folderCreated;
      if (folderCreated) {
        await this.places.bookmarks.remove(guid, {
          preventRemovalOfNonEmptyFolders: true,
        }).then(() => {
          folderRemoved = true;
        }).catch((cleanupError) => {
          console.error(
            "[WorkspaceBookmarks] Save-folder rollback failed",
            cleanupError,
          );
        });
      }
      if (folderRemoved) {
        await this.mutate((next) => {
          if (next.workspaceFolders[workspaceId] === guid) {
            if (existing) next.workspaceFolders[workspaceId] = existing;
            else delete next.workspaceFolders[workspaceId];
          }
          delete next.rules[guid];
        }).catch((cleanupError) => this.failStore(cleanupError));
      }
      throw error;
    }
  }

  async releaseCreatedWorkspaceFolder(
    result: { guid: string; created: boolean; workspaceId?: string },
  ): Promise<void> {
    if (!result.created) return;
    await this.refresh();
    const scope = this.getScope(result.guid);
    if (
      this.items.get(result.guid)?.type !== 2 ||
      (this.children.get(result.guid)?.length ?? 0) > 0 || !scope ||
      scope.length !== 1 ||
      (result.workspaceId !== undefined && scope[0] !== result.workspaceId)
    ) return;
    const workspaceId = scope[0];
    if (this.state.workspaceFolders[workspaceId] !== result.guid) return;
    try {
      await this.places.bookmarks.remove(result.guid, {
        preventRemovalOfNonEmptyFolders: true,
      });
    } catch (error) {
      // Another window may have saved into this folder since the empty check.
      // Keep both the bookmark data and its restrictive rule in that case.
      console.warn("[WorkspaceBookmarks] Save folder is still in use", error);
      return;
    }
    await this.mutate((next) => {
      delete next.rules[result.guid];
      if (next.workspaceFolders[workspaceId] === result.guid) {
        delete next.workspaceFolders[workspaceId];
      }
    });
    await this.refresh();
  }

  async exportSettings(
    options: { workspaceNames?: Record<string, string> } = {},
  ): Promise<WorkspaceBookmarkBackup> {
    await this.ready;
    if (this.storeError || this.state.pendingCopy) {
      throw new Error(
        this.storeError ?? "A bookmark copy is still in progress",
      );
    }
    return {
      ...parseWorkspaceBookmarkState(this.state),
      kind: "floorp-workspace-bookmark-scopes",
      createdAt: Date.now(),
      workspaceNames: { ...options.workspaceNames },
    };
  }

  async readRecoverySettings(): Promise<WorkspaceBookmarkBackup | null> {
    const value = await this.storage.readRecovery();
    if (value === null) return null;
    return {
      ...parseWorkspaceBookmarkState(value),
      kind: "floorp-workspace-bookmark-scopes",
      createdAt: Date.now(),
      workspaceNames: {},
    };
  }

  private async matchesLocalRecovery(
    settings: WorkspaceBookmarkBackup,
  ): Promise<boolean> {
    if (!this.storeError) return false;
    try {
      const recovery = await this.storage.readRecovery();
      return recovery !== null &&
        stateFingerprint(parseWorkspaceBookmarkState(recovery)) ===
          stateFingerprint(parseWorkspaceBookmarkState(settings));
    } catch {
      return false;
    }
  }

  async previewImportSettings(
    raw: unknown,
    knownIds: readonly string[] = this.knownWorkspaces(),
    mapping: WorkspaceBookmarkImportMapping = {},
  ): Promise<WorkspaceBookmarkImportPreview> {
    await this.ready;
    const settings = parseBackup(raw);
    const localRecovery = await this.matchesLocalRecovery(settings);
    const knownWorkspaceIds = Object.freeze(workspaceIds(knownIds));
    const known = new Set(knownWorkspaceIds);
    const unresolvedFolders = new Set<string>();
    const unresolvedWorkspaces = new Set<string>();
    const applicableRules: WorkspaceBookmarkImportPreview["applicableRules"] =
      [];
    const changes: WorkspaceBookmarkImportPreview["changes"] = [];
    for (const [oldGuid, ids] of Object.entries(settings.rules)) {
      const guid = mapping.folders?.[oldGuid] ?? oldGuid;
      const folder = this.items.get(guid);
      const mappedIds = ids.map((id) => mapping.workspaces?.[id] ?? id);
      let unresolved = false;
      for (let index = 0; index < mappedIds.length; index++) {
        if (
          !localRecovery && (
            !known.has(mappedIds[index]) || !WORKSPACE_ID.test(mappedIds[index])
          )
        ) {
          unresolvedWorkspaces.add(ids[index]);
          unresolved = true;
        }
      }
      if (
        !localRecovery && (!folder || folder.type !== 2 || ROOT_GUIDS.has(guid))
      ) {
        unresolvedFolders.add(oldGuid);
        continue;
      }
      if (unresolved) continue;
      const after = workspaceIds(mappedIds);
      applicableRules.push({ guid, workspaceIds: after });
      const before = this.getScope(guid);
      if (JSON.stringify(before) !== JSON.stringify(after)) {
        changes.push({
          guid,
          title: folder?.title ?? guid,
          before: before ? [...before] : null,
          after,
        });
      }
    }
    if (this.storeError && this.state.pendingCopy) {
      const knownFolders = new Set(this.state.pendingCopy.knownFolders);
      const assigned = new Set(applicableRules.map((rule) => rule.guid));
      for (const item of this.items.values()) {
        if (
          item.type === 2 && !ROOT_GUIDS.has(item.guid) &&
          !knownFolders.has(item.guid) &&
          !assigned.has(item.guid) && !this.state.rules[item.guid]
        ) {
          applicableRules.push({ guid: item.guid, workspaceIds: [] });
          changes.push({
            guid: item.guid,
            title: item.title,
            before: null,
            after: [],
          });
        }
      }
    }
    return {
      generation: this.generation,
      knownWorkspaceIds,
      settings,
      applicableRules,
      unresolvedFolders: [...unresolvedFolders],
      unresolvedWorkspaces: [...unresolvedWorkspaces],
      changes,
      sourceMatches: settings.sourceId === this.state.sourceId,
    };
  }

  async applyImportSettings(
    preview: WorkspaceBookmarkImportPreview,
    mapping: WorkspaceBookmarkImportMapping = {},
  ): Promise<WorkspaceBookmarkImportPreview> {
    if (this.activeTransfers.size > 0) {
      throw new Error("A bookmark operation is still in progress");
    }
    // Do not trust a mutable preview or assume GUIDs survived the confirmation.
    const expectedGeneration = preview.generation;
    const expectedWorkspaces = JSON.stringify(
      workspaceIds(preview.knownWorkspaceIds),
    );
    const isCurrentPreview = (): boolean =>
      expectedGeneration === this.generation &&
      expectedWorkspaces ===
        JSON.stringify(workspaceIds(this.knownWorkspaces()));
    if (!isCurrentPreview()) {
      throw new Error("Bookmark settings changed after the import preview");
    }
    const latest = await this.previewImportSettings(
      preview.settings,
      this.knownWorkspaces(),
      mapping,
    );
    const localRecovery = await this.matchesLocalRecovery(latest.settings);
    if (this.storeError) {
      if (
        !localRecovery &&
        (latest.unresolvedFolders.length > 0 ||
          latest.unresolvedWorkspaces.length > 0)
      ) {
        throw new Error(
          "Resolve all references before replacing unavailable bookmark settings",
        );
      }
    }
    await this.mutate((next) => {
      if (!isCurrentPreview()) {
        throw new Error("Bookmark settings changed after the import preview");
      }
      for (const entry of latest.applicableRules) {
        next.rules[entry.guid] = entry.workspaceIds;
      }
      delete next.pendingCopy;
      for (
        const [oldId, oldGuid] of Object.entries(
          latest.settings.workspaceFolders,
        )
      ) {
        const id = mapping.workspaces?.[oldId] ?? oldId;
        const guid = mapping.folders?.[oldGuid] ?? oldGuid;
        if (
          localRecovery || (this.knownWorkspaces().includes(id) &&
            this.items.get(guid)?.type === 2 &&
            latest.applicableRules.some((rule) => rule.guid === guid))
        ) {
          next.workspaceFolders[id] = guid;
        }
      }
    }, true);
    return latest;
  }

  private subtree(guid: string): WorkspaceBookmarkTree {
    const item = this.items.get(guid);
    if (!item) throw new Error("Bookmark was removed");
    return {
      guid,
      typeCode: item.type,
      children: (this.children.get(guid) ?? []).map((child) =>
        this.subtree(child)
      ),
    };
  }

  async captureTransferScopes(
    items: readonly (string | {
      guid?: string;
      itemGuid?: string;
      bookmarkGuid?: string;
    })[],
    targetParentGuid: string,
    doCopy: boolean,
    context: WorkspaceBookmarkContext,
  ): Promise<WorkspaceBookmarkTransferPlan> {
    await this.ready;
    if (!this.isCurrentContext(context)) {
      throw new Error("Bookmark context changed");
    }
    const target = this.items.get(targetParentGuid);
    if (!target || target.type !== 2) {
      throw new Error("Bookmark target folder was removed");
    }
    const sourceGuids = [
      ...new Set(
        items.map((item) =>
          typeof item === "string"
            ? item
            : item.guid ?? item.itemGuid ?? item.bookmarkGuid ?? ""
        ).filter((guid) => GUID.test(guid) && this.items.has(guid)),
      ),
    ];
    if (
      !this.isVisible(targetParentGuid, context) ||
      sourceGuids.some((guid) => !this.isVisible(guid, context))
    ) {
      throw new Error("The bookmark transfer is outside the current workspace");
    }
    const explicitRules: Record<string, string[]> = Object.create(null);
    const sourceTrees: Record<string, WorkspaceBookmarkTree> = Object.create(
      null,
    );
    let requiresConfirmation = false;
    let hiddenDescendantCount = 0;
    const targetScope = this.getEffectiveScope(targetParentGuid).workspaceIds;
    const visit = (guid: string, inherited: readonly string[] | null): void => {
      const item = this.items.get(guid);
      if (!item) throw new Error("Bookmark source was removed");
      const rule = this.state.rules[guid];
      if (rule) explicitRules[guid] = [...rule];
      const before = this.getEffectiveScope(guid).workspaceIds;
      const after = inherited === null
        ? rule ?? null
        : rule
        ? inherited.filter((id) => rule.includes(id))
        : inherited;
      if (
        context.enabled && before !== null &&
        (after === null || after.some((id) => !before.includes(id)))
      ) requiresConfirmation = true;
      if (!this.isVisible(guid, context)) hiddenDescendantCount++;
      for (const child of this.children.get(guid) ?? []) visit(child, after);
    };
    for (const guid of sourceGuids) {
      sourceTrees[guid] = this.subtree(guid);
      visit(guid, targetScope);
    }
    return {
      context,
      sourceGuids,
      targetParentGuid,
      doCopy,
      requiresConfirmation,
      hiddenDescendantCount,
      explicitRules,
      sourceTrees,
    };
  }

  async applyTransferScopes(
    plan: WorkspaceBookmarkTransferPlan,
    mapping: Readonly<Record<string, string>>,
  ): Promise<void> {
    try {
      if (!this.activeTransfers.has(plan)) {
        throw new Error("The bookmark transfer was not reserved");
      }
      await this.refresh();
      if (!plan.doCopy) return;
      const assignments: Record<string, string[]> = Object.create(null);
      const pair = (
        source: WorkspaceBookmarkTree,
        targetGuid: string,
      ): void => {
        const target = this.items.get(targetGuid);
        const targets = this.children.get(targetGuid) ?? [];
        const sources = source.children ?? [];
        if (
          !target || target.type !== source.typeCode ||
          targets.length !== sources.length
        ) {
          throw new Error(
            "The copied bookmark tree changed before scope assignment",
          );
        }
        if (source.guid === targetGuid) {
          throw new Error("A bookmark copy did not allocate a new GUID");
        }
        const rule = plan.explicitRules[source.guid];
        if (rule) assignments[targetGuid] = [...rule];
        sources.forEach((child, index) => pair(child, targets[index]));
      };
      for (const source of plan.sourceGuids) {
        const targetGuid = mapping[source];
        if (!targetGuid) {
          throw new Error("The copy operation did not return a GUID mapping");
        }
        pair(plan.sourceTrees[source], targetGuid);
      }
      await this.mutate((next) => {
        for (const [targetGuid, ids] of Object.entries(assignments)) {
          next.rules[targetGuid] = [...ids];
        }
        delete next.pendingCopy;
      });
    } catch (error) {
      // A partially assigned copy must not silently become globally visible.
      this.failStore(error);
      throw error;
    } finally {
      if (this.activeTransfers.delete(plan)) this.changed();
    }
  }

  async beginTransferScopes(
    plan: WorkspaceBookmarkTransferPlan,
  ): Promise<void> {
    if (!this.isCurrentContext(plan.context) || this.activeTransfers.size > 0) {
      throw new Error("Bookmark context changed before transfer");
    }
    this.activeTransfers.add(plan);
    this.changed();
    if (!plan.doCopy || Object.keys(plan.explicitRules).length === 0) return;
    const reservedGeneration = this.generation;
    const selectionIsCurrent = (): boolean => {
      const context = plan.context;
      const window = context.windowId === null
        ? null
        : this.windowsById.get(context.windowId)?.deref();
      if (context.windowId !== null && (!window || window.closed)) return false;
      const current = this.getContext(window ?? null, {
        allItems: context.allItems,
      });
      return current.windowId === context.windowId &&
        current.windowGeneration === context.windowGeneration &&
        current.workspaceId === context.workspaceId &&
        current.enabled === context.enabled &&
        current.scopeEnabled === context.scopeEnabled;
    };
    try {
      await this.mutate((next) => {
        if (this.generation !== reservedGeneration || !selectionIsCurrent()) {
          throw new Error("Bookmark context changed while reserving the copy");
        }
        next.pendingCopy = {
          knownFolders: [...this.items.values()].filter((item) =>
            item.type === 2
          ).map((item) => item.guid).sort(),
        };
      });
      if (this.generation !== reservedGeneration + 1 || !selectionIsCurrent()) {
        throw new Error("Bookmark context changed before the native copy");
      }
    } catch (error) {
      // Native copying has not started. Removing our marker is safe here.
      if (this.state.pendingCopy && !this.storeError) {
        await this.mutate((next) => {
          delete next.pendingCopy;
        }).catch((cleanupError) => this.failStore(cleanupError));
      }
      this.activeTransfers.delete(plan);
      this.changed();
      throw error;
    }
  }

  abortTransferScopes(
    plan: WorkspaceBookmarkTransferPlan,
    error?: unknown,
  ): void {
    if (!this.activeTransfers.delete(plan)) return;
    if (plan.doCopy && (error || this.state.pendingCopy)) {
      this.failStore(
        error ??
          "A bookmark copy was cancelled before its workspace rules were saved",
      );
    } else this.changed();
  }
}

export const WorkspaceBookmarks = new WorkspaceBookmarksService();
