// SPDX-License-Identifier: MPL-2.0

import { createSignal, onCleanup } from "solid-js";
import i18next from "i18next";
import type {
  WorkspaceSyncDocument,
  WorkspaceSyncProvider,
  WorkspaceSyncSnapshot,
} from "#modules/common/workspace-sync-types.ts";
import {
  isSyncableWorkspaceURL,
  isWorkspaceSyncId,
  liveWorkspaceSyncTabs,
  WORKSPACE_SYNC_CHANGED,
  WORKSPACE_SYNC_ENGINE_PREF,
} from "#modules/common/workspace-sync.ts";
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
  IncomingWorkspaceNavigation,
  SyncSessionStore,
  WorkspaceSyncAPI,
  WorkspaceSyncBrowser,
  WorkspaceSyncProgressFlags,
  WorkspaceSyncProgressListener,
  WorkspaceSyncTabBrowser,
} from "./workspacesSyncTypes.ts";

const TAB_ID_KEY = "floorp-workspace-sync-id";
const TAB_OWNER_KEY = "floorp-workspace-sync-owner";
const TAB_PENDING_URL_KEY = "floorp-workspace-sync-url";
const PROGRESS_FLAGS = Ci
  .nsIWebProgressListener as unknown as WorkspaceSyncProgressFlags;
const HISTORY_LOAD_COMMAND = Ci.nsIDocShell.LOAD_CMD_HISTORY as number;
const PUSHSTATE_LOAD_COMMAND = Ci.nsIDocShell.LOAD_CMD_PUSHSTATE as number;

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
  private physicalOrder = new Map<string, string[]>();
  private incomingNavigations = new Map<
    XULElement,
    IncomingWorkspaceNavigation
  >();

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
      if (event?.type === "TabClose" && event.target) {
        this.incomingNavigations.get(event.target as XULElement)?.finish();
      }
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
    const progressListener: WorkspaceSyncProgressListener = {
      onLocationChange: (browser, progress, request, location, flags) => {
        if (progress.isTopLevel) {
          const incoming = this.navigationFor(browser);
          if (incoming) {
            const requested = this.requestURL(request);
            // The old page can change its URL while this load is pending.
            // Native loadType distinguishes push/replaceState from Back/Forward.
            const owned = requested === incoming.url ||
              (location.spec === incoming.url &&
                !(progress.loadType & PUSHSTATE_LOAD_COMMAND));
            if (
              owned ||
              (progress.loadType & HISTORY_LOAD_COMMAND)
            ) {
              incoming.finish(
                owned &&
                  (flags & PROGRESS_FLAGS.LOCATION_CHANGE_ERROR_PAGE)
                  ? new Error("Incoming workspace navigation failed")
                  : undefined,
              );
            }
          }
        }
        this.scheduleCapture();
      },
      onStateChange: (browser, progress, request, flags, _status) => {
        if (!progress.isTopLevel) return;
        const incoming = this.navigationFor(browser);
        const requested = this.requestURL(request);
        if (!incoming) {
          if (
            requested && (flags & PROGRESS_FLAGS.STATE_START) &&
            (flags & PROGRESS_FLAGS.STATE_IS_DOCUMENT)
          ) {
            const tab = [...gBrowser.tabs].find((tab) =>
              this.tabBrowser.getBrowserForTab(tab) === browser
            );
            if (tab) {
              const pending = this.sessionStore.getCustomTabValue(
                tab,
                TAB_PENDING_URL_KEY,
              );
              // A real navigation abandons a failed incoming blank tab's retry
              // target. An initial blank location notification alone does not.
              if (pending && pending !== requested) {
                this.sessionStore.setCustomTabValue(
                  tab,
                  TAB_PENDING_URL_KEY,
                  "",
                );
              }
            }
          }
          return;
        }
        if (
          (flags & PROGRESS_FLAGS.STATE_START) &&
          (flags & PROGRESS_FLAGS.STATE_IS_DOCUMENT) &&
          requested && requested !== incoming.url
        ) {
          incoming.finish();
        } else if (
          (flags & PROGRESS_FLAGS.STATE_STOP) &&
          (flags &
            (PROGRESS_FLAGS.STATE_IS_NETWORK |
              PROGRESS_FLAGS.STATE_IS_DOCUMENT)) &&
          requested === incoming.url
        ) {
          incoming.checkStopped(progress);
        }
      },
    };
    for (const event of events) {
      gBrowser.tabContainer.addEventListener(event, onChange);
    }
    this.tabBrowser.addTabsProgressListener(progressListener);
    const workspaceMoves = new MutationObserver(() => this.scheduleCapture());
    workspaceMoves.observe(gBrowser.tabContainer as Node, {
      attributes: true,
      attributeFilter: [WORKSPACE_TAB_ATTRIBUTION_ID],
      subtree: true,
    });
    Services.prefs.addObserver(WORKSPACE_DATA_PREF_NAME, onPrefsChange);
    Services.obs.addObserver(onSyncChange, WORKSPACE_SYNC_CHANGED);
    Services.prefs.addObserver(WORKSPACE_SYNC_ENGINE_PREF, onSyncChange);

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
        retryTimer = this.window.setTimeout(initialize, retryDelay);
        retryDelay = Math.min(retryDelay * 2, 30_000);
      }
    };
    initialize();

    onCleanup(() => {
      this.stopped = true;
      for (const incoming of this.incomingNavigations.values()) {
        incoming.cancel(new Error("Workspace window is shutting down"));
      }
      registration.abort();
      this.window.clearTimeout(retryTimer);
      unregister?.();
      this.window.clearTimeout(this.timer);
      for (const event of events) {
        gBrowser.tabContainer.removeEventListener(event, onChange);
      }
      this.tabBrowser.removeTabsProgressListener(progressListener);
      workspaceMoves.disconnect();
      Services.prefs.removeObserver(WORKSPACE_DATA_PREF_NAME, onPrefsChange);
      Services.obs.removeObserver(onSyncChange, WORKSPACE_SYNC_CHANGED);
      Services.prefs.removeObserver(WORKSPACE_SYNC_ENGINE_PREF, onSyncChange);
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

  isEnabled(): boolean {
    this.revision[0]();
    return Services.prefs.getBoolPref(WORKSPACE_SYNC_ENGINE_PREF, true);
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
    await this.api.deleteWorkspace(id);
  }

  private scheduleCapture(): void {
    if (!this.active || this.applying || this.stopped) return;
    this.window.clearTimeout(this.timer);
    this.timer = this.window.setTimeout(() => {
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

  private get tabBrowser(): WorkspaceSyncTabBrowser {
    return gBrowser as unknown as WorkspaceSyncTabBrowser;
  }

  private navigationFor(
    browser: WorkspaceSyncBrowser,
  ): IncomingWorkspaceNavigation | undefined {
    for (const [tab, incoming] of this.incomingNavigations) {
      if (this.tabBrowser.getBrowserForTab(tab) === browser) return incoming;
    }
    return undefined;
  }

  private requestURL(request: nsIRequest | null): string | undefined {
    try {
      return request?.QueryInterface?.(Ci.nsIChannel).originalURI?.spec;
    } catch {
      return undefined;
    }
  }

  private navigateIncoming(
    tab: XULElement,
    url: string,
    title: string,
    signal?: AbortSignal,
    alreadyLoading = false,
  ): IncomingWorkspaceNavigation {
    const uri = Services.io.newURI(url);
    const previous = this.incomingNavigations.get(tab);
    if (previous?.url === uri.spec && previous.signal === signal) {
      previous.title = title;
      return previous;
    }
    previous?.cancel(
      new Error("Incoming workspace navigation was replaced"),
    );
    let resolve!: () => void;
    let reject!: (error: Error) => void;
    const promise = new Promise<void>((done, fail) => {
      resolve = done;
      reject = fail;
    });
    // A bounded apply may return before this navigation ends. Keep its eventual
    // failure handled even when no apply is currently waiting for it.
    void promise.catch(() => {});
    let stoppedTimer: number | undefined;
    const abort = () =>
      incoming.cancel(new Error("Workspace Sync account changed"));
    const incoming: IncomingWorkspaceNavigation = {
      url: uri.spec,
      title,
      signal,
      promise,
      finish: (error) => {
        if (this.incomingNavigations.get(tab) !== incoming) return;
        this.incomingNavigations.delete(tab);
        this.window.clearTimeout(stoppedTimer);
        signal?.removeEventListener("abort", abort);
        try {
          if (
            this.sessionStore.getCustomTabValue(tab, TAB_PENDING_URL_KEY) ===
              url &&
            !(error &&
              this.tabBrowser.getBrowserForTab(tab).currentURI?.spec ===
                "about:blank")
          ) this.sessionStore.setCustomTabValue(tab, TAB_PENDING_URL_KEY, "");
        } catch (error) {
          console.error("[WorkspacesSync] Could not clear pending URL", error);
        }
        if (error) reject(error);
        else {
          resolve();
          this.scheduleCapture();
        }
      },
      cancel: (error) => {
        if (this.incomingNavigations.get(tab) !== incoming) return;
        incoming.finish(error);
        try {
          this.tabBrowser.getBrowserForTab(tab).stop(
            Ci.nsIWebNavigation.STOP_ALL as number,
          );
        } catch (stopError) {
          console.error(
            "[WorkspacesSync] Could not stop incoming load",
            stopError,
          );
        }
      },
      checkStopped: (progress) => {
        this.window.clearTimeout(stoppedTimer);
        stoppedTimer = this.window.setTimeout(() => {
          if (!progress.isLoadingDocument) {
            incoming.finish(
              gBrowser.getBrowserForTab(tab).currentURI?.spec === incoming.url
                ? undefined
                : new Error("Incoming workspace navigation did not commit"),
            );
          }
        }, 0);
      },
    };
    this.incomingNavigations.set(tab, incoming);
    signal?.addEventListener("abort", abort, { once: true });
    try {
      if (signal?.aborted) abort();
      else if (!alreadyLoading) {
        gBrowser.getBrowserForTab(tab).loadURI(uri, {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        });
      }
    } catch (error) {
      incoming.finish(
        error instanceof Error ? error : new Error(String(error)),
      );
    }
    return incoming;
  }

  private async waitForIncoming(
    navigations: IncomingWorkspaceNavigation[],
  ): Promise<void> {
    if (!navigations.length) return;
    let timer: number | undefined;
    try {
      await Promise.race([
        Promise.all(navigations.map((incoming) => incoming.promise)),
        new Promise<never>((_, reject) => {
          timer = this.window.setTimeout(
            () => reject(new Error("Incoming workspace navigation timed out")),
            30_000,
          );
        }),
      ]);
    } finally {
      this.window.clearTimeout(timer);
    }
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

  private syncableURL(tab: XULElement): string | undefined {
    const incoming = this.incomingNavigations.get(tab);
    if (incoming) return incoming.url;
    const current = gBrowser.getBrowserForTab(tab).currentURI?.spec;
    if (isSyncableWorkspaceURL(current)) return current;
    // Explicit internal/file navigation must not fall back to stale HTTP
    // history. Only a restoring tab or an incoming tab still loading may use
    // SessionStore's pending public URL.
    if (current && current !== "about:blank") return undefined;
    const pending = this.sessionStore.getCustomTabValue(
      tab,
      TAB_PENDING_URL_KEY,
    );
    if (!tab.hasAttribute("pending") && !isSyncableWorkspaceURL(pending)) {
      return undefined;
    }
    try {
      const state = JSON.parse(
        this.sessionStore.getTabState(tab),
      ) as Record<string, unknown>;
      const restored = extractUrlFromState(state);
      if (isSyncableWorkspaceURL(restored)) return restored;
    } catch (error) {
      console.error("[WorkspacesSync] Could not read pending tab", error);
    }
    return isSyncableWorkspaceURL(pending) ? pending : undefined;
  }

  capture(id: string, seenIds: Set<string>): WorkspaceSyncSnapshot | null {
    const workspace = this.ctx.getRawWorkspace(id as TWorkspaceID);
    if (!workspace) return null;
    const tabs: WorkspaceSyncSnapshot["tabs"] = [];
    for (const tab of this.tabs(id)) {
      const previousId = this.sessionStore.getCustomTabValue(tab, TAB_ID_KEY);
      const previousOwner = this.sessionStore.getCustomTabValue(
        tab,
        TAB_OWNER_KEY,
      );
      const moved = Boolean(previousOwner && previousOwner !== id);
      if (
        (!moved && this.api.isPendingClose(previousId)) ||
        (!moved && this.api.isRetired(previousId) &&
          !this.explicitlyOpened.has(tab))
      ) continue;
      const browser = gBrowser.getBrowserForTab(tab);
      const url = this.syncableURL(tab);
      const known = this.api.getSyncedTab(id, previousId);
      if (!isSyncableWorkspaceURL(url)) {
        // The physical tab remains open. Preserve its previous public record,
        // including its safe title, rather than tombstoning it or exporting an
        // internal page's title. A new/moved internal tab has no public record.
        if (known && this.tabId(tab, seenIds) === known.id) tabs.push(known);
        continue;
      }
      if (browser.currentURI?.spec === url) {
        this.sessionStore.setCustomTabValue(tab, TAB_PENDING_URL_KEY, "");
      }
      tabs.push({
        id: this.tabId(tab, seenIds),
        url,
        title: this.incomingNavigations.get(tab)?.title ??
          (isSyncableWorkspaceURL(browser.currentURI?.spec)
            ? (tab.getAttribute("label") ?? "").slice(0, 1024).toWellFormed()
            : known?.title ?? ""),
        pinned: tab.hasAttribute("pinned"),
      });
      this.explicitlyOpened.delete(tab);
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

  async apply(
    document: WorkspaceSyncDocument,
    missingTabIds: ReadonlySet<string>,
    signal?: AbortSignal,
  ): Promise<void> {
    if (signal?.aborted) {
      throw new Error("Workspace Sync account changed");
    }
    if (this.stopped) throw new Error("Workspace window is shutting down");
    const navigations: IncomingWorkspaceNavigation[] = [];
    let completed = false;
    this.applying = true;
    this.window.clearTimeout(this.timer);
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
        let created = false;
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
          created = true;
        }
        if (!tab) continue;
        if (!this.syncableURL(tab)) {
          // Keep an existing local internal/file page and its local pin/order
          // state. Explicit remote tab tombstones were handled above.
          previousTab = tab;
          continue;
        }
        const browser = gBrowser.getBrowserForTab(tab);
        const remoteSpec = Services.io.newURI(remoteTab.url).spec;
        const incoming = this.incomingNavigations.get(tab);
        if (incoming && incoming.url !== remoteSpec) {
          // A newer remote value may equal the still-visible old page. Stop the
          // superseded owned load before accepting that already-current URL.
          incoming.cancel(
            new Error("Incoming workspace navigation was replaced"),
          );
        }
        if (
          browser.currentURI?.spec !== remoteSpec ||
          this.incomingNavigations.has(tab)
        ) {
          navigations.push(
            this.navigateIncoming(
              tab,
              remoteTab.url,
              remoteTab.title,
              signal,
              created,
            ),
          );
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
      this.applying = false;
      await this.waitForIncoming(navigations);
      completed = true;
    } finally {
      this.applying = false;
      if (completed) this.scheduleCapture();
    }
  }
}
