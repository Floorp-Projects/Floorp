// SPDX-License-Identifier: MPL-2.0

import { createSignal, onCleanup } from "solid-js";
import i18next from "i18next";
import type {
  WorkspaceSyncDocument,
  WorkspaceSyncProvider,
  WorkspaceSyncSnapshot,
} from "../../../modules/common/workspace-sync-types.ts";
import {
  isSyncableWorkspaceURL,
  isWorkspaceSyncId,
  liveWorkspaceSyncTabs,
  WORKSPACE_SYNC_CHANGED,
} from "../../../modules/common/workspace-sync.ts";
import type { WorkspacesService } from "./workspacesService.ts";
import { setWorkspacesDataStore, workspacesDataStore } from "./data/data.ts";
import type {
  TWorkspace,
  TWorkspaceID,
  TWorkspacesStoreData,
} from "./utils/type.ts";
import {
  WORKSPACE_DATA_PREF_NAME,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "./utils/workspaces-static-names.ts";
import { extractUrlFromState } from "./utils/workspace-snapshot.ts";

import type {
  SyncSessionStore,
  WorkspaceSyncAPI,
} from "./workspacesSyncTypes.ts";

const TAB_ID_KEY = "floorp-workspace-sync-id";
const TAB_OWNER_KEY = "floorp-workspace-sync-owner";
const TAB_PENDING_URL_KEY = "floorp-workspace-sync-url";

/** Adapts window-local browser tabs to the profile's encrypted Sync collection. */
export class WorkspacesSyncController implements WorkspaceSyncProvider {
  window = window;
  private api: WorkspaceSyncAPI;
  private sessionStore: SyncSessionStore;
  private applying = false;
  private stopped = false;
  private active = false;
  private privateWindow = false;
  private timer: number | undefined;
  private revision = createSignal(0);
  private explicitlyOpened = new WeakSet<XULElement>();
  private openedAfterStartup = new WeakSet<XULElement>();
  private locallyExcluded = new WeakSet<XULElement>();
  private physicalOrder = new Map<string, string[]>();

  constructor(private ctx: WorkspacesService) {
    const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
    );
    this.api = (ChromeUtils.importESModule(
      "resource://noraneko/modules/workspaces/FloorpWorkspacesSync.sys.mjs",
    ) as { FloorpWorkspacesSync: WorkspaceSyncAPI }).FloorpWorkspacesSync;
    this.sessionStore = (ChromeUtils.importESModule(
      "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
    ) as { SessionStore: SyncSessionStore }).SessionStore;
    this.privateWindow = PrivateBrowsingUtils.isWindowPrivate(window);
    if (this.privateWindow) return;

    const onChange = (event?: Event) => {
      if (
        event?.type === "TabOpen" && this.active && !this.applying &&
        event.target
      ) {
        this.explicitlyOpened.add(event.target as XULElement);
        this.openedAfterStartup.add(event.target as XULElement);
      }
      if (
        event?.type === "SSTabRestored" && this.active && !this.applying &&
        event.target && this.openedAfterStartup.has(event.target as XULElement)
      ) {
        // Undo Close may restore extData after an earlier capture. Only tabs
        // explicitly opened in this session may replace a retired UUID.
        const tab = event.target as XULElement;
        this.explicitlyOpened.add(tab);
        const previous = this.sessionStore.getCustomTabValue(tab, TAB_ID_KEY);
        if (
          this.api.isRetired(previous) && !this.api.isPendingClose(previous)
        ) this.tabId(tab, new Set());
      }
      this.scheduleCapture();
    };
    const onPrefsChange = () => this.scheduleCapture();
    const onSyncChange = () => this.revision[1]((value) => value + 1);
    const events = [
      "TabOpen",
      "TabClose",
      "TabMove",
      "TabPinned",
      "TabUnpinned",
      "TabAttrModified",
      "SSTabRestored",
    ];
    const progressListener = { onLocationChange: () => this.scheduleCapture() };
    for (const event of events) {
      gBrowser.tabContainer.addEventListener(event, onChange);
    }
    gBrowser.addTabsProgressListener(progressListener);
    const workspaceMoves = new MutationObserver(() => this.scheduleCapture());
    workspaceMoves.observe(gBrowser.tabContainer as Node, {
      attributes: true,
      attributeFilter: [WORKSPACE_TAB_ATTRIBUTION_ID],
      subtree: true,
    });
    Services.prefs.addObserver(WORKSPACE_DATA_PREF_NAME, onPrefsChange);
    Services.obs.addObserver(onSyncChange, WORKSPACE_SYNC_CHANGED);

    const registration = new AbortController();
    let unregister: (() => void) | undefined;
    let retryDelay = 1000;
    let retryTimer: number | undefined;
    const initialize = async () => {
      try {
        await this.sessionStore.promiseAllWindowsRestored;
        if (this.stopped) return;
        unregister = await this.api.register(this, registration.signal);
        if (this.stopped) unregister();
        else {
          this.active = true;
          onSyncChange();
        }
      } catch (error) {
        if (this.stopped) return;
        console.error(
          "[WorkspacesSync] Initialization failed; retrying",
          error,
        );
        retryTimer = globalThis.setTimeout(initialize, retryDelay);
        retryDelay = Math.min(retryDelay * 2, 30_000);
      }
    };
    initialize();

    onCleanup(() => {
      this.stopped = true;
      registration.abort();
      globalThis.clearTimeout(retryTimer);
      unregister?.();
      globalThis.clearTimeout(this.timer);
      for (const event of events) {
        gBrowser.tabContainer.removeEventListener(event, onChange);
      }
      gBrowser.removeTabsProgressListener(progressListener);
      workspaceMoves.disconnect();
      Services.prefs.removeObserver(WORKSPACE_DATA_PREF_NAME, onPrefsChange);
      Services.obs.removeObserver(onSyncChange, WORKSPACE_SYNC_CHANGED);
    });
  }

  isAvailable(): boolean {
    this.revision[0]();
    return this.active;
  }

  isSelected(id: TWorkspaceID): boolean {
    this.revision[0]();
    return this.active && this.api.isSelected(id);
  }

  async select(id: TWorkspaceID, selected: boolean): Promise<void> {
    if (!this.active) return;
    try {
      await this.api.select(id, selected);
    } catch (error) {
      console.error(
        "[WorkspacesSync] Could not change workspace selection",
        error,
      );
      Services.prompt.alert(
        window as unknown as mozIDOMWindowProxy,
        i18next.t("workspaces.sync.error-title"),
        this.errorText(id) ?? i18next.t("workspaces.sync.invalid-workspace"),
      );
    }
  }

  errorText(id: TWorkspaceID): string | null {
    this.revision[0]();
    const code = this.api.errorFor(id);
    return code ? i18next.t(`workspaces.sync.${code}`) : null;
  }

  async deleteWorkspace(id: TWorkspaceID): Promise<void> {
    if (!this.privateWindow) await this.api.deleteWorkspace(id);
  }

  private scheduleCapture(): void {
    if (!this.active || this.applying || this.stopped) return;
    globalThis.clearTimeout(this.timer);
    this.timer = globalThis.setTimeout(() => {
      this.api.capture().catch((error: unknown) =>
        console.error("[WorkspacesSync] Capture failed", error)
      );
    }, 750);
  }

  private tabs(id: string): XULElement[] {
    return [...gBrowser.tabs].filter((tab) =>
      tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID) === id
    );
  }

  private tabId(tab: XULElement, seenIds?: Set<string>): string {
    const previous = this.sessionStore.getCustomTabValue(tab, TAB_ID_KEY);
    const owner = tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID) ?? "";
    const previousOwner = this.sessionStore.getCustomTabValue(
      tab,
      TAB_OWNER_KEY,
    );
    if (
      isWorkspaceSyncId(previous) && !seenIds?.has(previous) &&
      (!seenIds ||
        (!this.api.isRetired(previous) &&
          (!previousOwner || previousOwner === owner)))
    ) {
      if (seenIds) {
        this.sessionStore.setCustomTabValue(tab, TAB_OWNER_KEY, owner);
      }
      seenIds?.add(previous);
      return previous;
    }
    const id = Services.uuid.generateUUID().toString().replace(/[{}]/g, "");
    this.sessionStore.setCustomTabValue(tab, TAB_ID_KEY, id);
    this.sessionStore.setCustomTabValue(tab, TAB_OWNER_KEY, owner);
    seenIds?.add(id);
    return id;
  }

  tabIds(id: string): string[] {
    return this.tabs(id).map((tab) => this.tabId(tab));
  }

  capture(id: string, seenIds: Set<string>): WorkspaceSyncSnapshot | null {
    const workspace = this.ctx.getRawWorkspace(id as TWorkspaceID);
    if (!workspace) return null;
    const tabs: WorkspaceSyncSnapshot["tabs"] = [];
    for (const tab of this.tabs(id)) {
      const previousId = this.sessionStore.getCustomTabValue(tab, TAB_ID_KEY);
      if (
        this.api.isPendingClose(previousId) ||
        (this.api.isRetired(previousId) && !this.explicitlyOpened.has(tab))
      ) continue;
      const browser = gBrowser.getBrowserForTab(tab);
      let url = browser.currentURI?.spec;
      if (!isSyncableWorkspaceURL(url)) {
        try {
          const state = JSON.parse(
            this.sessionStore.getTabState(tab),
          ) as Record<string, unknown>;
          url = extractUrlFromState(state) ?? undefined;
        } catch (error) {
          console.error("[WorkspacesSync] Could not read pending tab", error);
        }
      }
      if (!isSyncableWorkspaceURL(url)) {
        url = this.sessionStore.getCustomTabValue(tab, TAB_PENDING_URL_KEY);
      }
      if (!isSyncableWorkspaceURL(url)) {
        this.locallyExcluded.add(tab);
        continue;
      }
      if (browser.currentURI?.spec === url) {
        this.sessionStore.setCustomTabValue(tab, TAB_PENDING_URL_KEY, "");
      }
      tabs.push({
        id: this.tabId(tab, seenIds),
        url,
        title: (tab.getAttribute("label") ?? "").slice(0, 1024).toWellFormed(),
        pinned: tab.hasAttribute("pinned"),
      });
      this.explicitlyOpened.delete(tab);
      this.locallyExcluded.delete(tab);
    }
    const order = this.tabs(id).map((tab) => this.tabId(tab)).filter((tabId) =>
      !this.api.isRetired(tabId)
    );
    const baseline = this.physicalOrder.get(id);
    const orderChanged = baseline !== undefined &&
      (baseline.length !== order.length ||
        baseline.some((tabId, index) => tabId !== order[index]));
    this.physicalOrder.set(id, order);
    return {
      orderChanged,
      workspaceId: id,
      name: workspace.name,
      icon: workspace.icon ?? null,
      tabs,
    };
  }

  apply(
    document: WorkspaceSyncDocument,
    missingTabIds: ReadonlySet<string>,
  ): Promise<void> {
    this.applying = true;
    globalThis.clearTimeout(this.timer);
    try {
      const id = document.workspaceId as TWorkspaceID;
      const localContainerId = this.ctx.getRawWorkspace(id)?.userContextId ?? 0;
      setWorkspacesDataStore("data", (previous) => {
        const map = new Map(
          previous as unknown as Map<TWorkspaceID, TWorkspace>,
        );
        const existing = map.get(id);
        map.set(id, {
          ...existing,
          name: document.name,
          icon: document.icon,
          userContextId: existing?.userContextId ?? 0,
          isSelected: existing?.isSelected ?? null,
          isDefault: existing?.isDefault ?? null,
        });
        return map as unknown as TWorkspacesStoreData["data"];
      });
      if (!workspacesDataStore.order.includes(id)) {
        setWorkspacesDataStore("order", (previous) => [...previous, id]);
      }

      const local = new Map(this.tabs(id).map((tab) => [this.tabId(tab), tab]));
      const remote = new Map(document.tabs.map((tab) => [tab.id, tab]));
      for (const [tabId, tab] of local) {
        if (remote.get(tabId)?.deleted) {
          gBrowser.removeTab(tab, { animate: false });
        }
      }
      let previousTab: XULElement | undefined;
      for (const remoteTab of liveWorkspaceSyncTabs(document)) {
        let tab = local.get(remoteTab.id);
        if (!tab && missingTabIds.has(remoteTab.id)) {
          tab = this.ctx.withExplicitTabUserContext(
            localContainerId,
            () =>
              gBrowser.addTab(remoteTab.url, {
                inBackground: true,
                skipAnimation: true,
                userContextId: localContainerId,
                triggeringPrincipal: Services.scriptSecurityManager
                  .getSystemPrincipal(),
              }),
          );
          this.sessionStore.setCustomTabValue(tab, TAB_ID_KEY, remoteTab.id);
          this.sessionStore.setCustomTabValue(tab, TAB_OWNER_KEY, id);
          this.sessionStore.setCustomTabValue(
            tab,
            TAB_PENDING_URL_KEY,
            remoteTab.url,
          );
          this.ctx.tabManagerCtx.setWorkspaceIdToAttribute(tab, id);
        }
        if (!tab) continue;
        const browser = gBrowser.getBrowserForTab(tab);
        if (browser.currentURI?.spec !== remoteTab.url) {
          browser.loadURI(Services.io.newURI(remoteTab.url), {
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
          });
        }
        if (remoteTab.pinned && !tab.hasAttribute("pinned")) {
          gBrowser.pinTab(tab);
        }
        if (!remoteTab.pinned && tab.hasAttribute("pinned")) {
          gBrowser.unpinTab(tab);
        }
        if (previousTab) {
          const index = [...gBrowser.tabs].indexOf(previousTab);
          gBrowser.moveTabTo(tab, { tabIndex: index + 1 });
        }
        previousTab = tab;
      }
      this.ctx.tabManagerCtx.updateTabsVisibility();
      this.physicalOrder.set(
        id,
        this.tabs(id).map((tab) => this.tabId(tab)).filter((tabId) =>
          !this.api.isRetired(tabId)
        ),
      );
      return Promise.resolve();
    } finally {
      this.applying = false;
    }
  }
}
