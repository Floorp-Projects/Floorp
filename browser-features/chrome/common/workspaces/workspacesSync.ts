// SPDX-License-Identifier: MPL-2.0

import { createSignal, onCleanup } from "solid-js";
import i18next from "i18next";
import type {
  WorkspaceSyncApplyResult,
  WorkspaceSyncDocument,
  WorkspaceSyncProvider,
  WorkspaceSyncSnapshot,
} from "#modules/common/workspace-sync-types.ts";
import {
  isSyncableWorkspaceURL,
  isWorkspaceSyncId,
  liveWorkspaceSyncTabs,
  WORKSPACE_SYNC_CHANGED,
  WORKSPACE_SYNC_V2_ENGINE_PREF as WORKSPACE_SYNC_ENGINE_PREF,
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
  WORKSPACE_TAB_MOVED_EVENT,
} from "./utils/workspaces-static-names.ts";
import { extractUrlFromState } from "./utils/workspace-snapshot.ts";
import type { WorkspaceContainerRef } from "#modules/common/workspace-container-types.ts";
import { WORKSPACE_CONTAINER_BINDINGS_CHANGED } from "#modules/common/workspace-container-bindings.ts";
import { parseWorkspaceContainerRef } from "#modules/common/workspace-container.ts";
import {
  getWorkspaceContainerBindings,
  nativeWorkspaceContainerPrompt,
  setupWorkspaceContainers,
  workspaceContainerNeedsReopen,
  workspaceContainerPendingText,
} from "./workspace-container-ui.ts";

import type {
  IncomingWorkspaceNavigation,
  SyncSessionStore,
  WorkspaceContainerReopenState,
  WorkspaceSyncAPI,
  WorkspaceSyncBrowser,
  WorkspaceSyncProgressFlags,
  WorkspaceSyncProgressListener,
  WorkspaceSyncTabBrowser,
  WorkspaceTabMoveDetail,
} from "./workspacesSyncTypes.ts";

const TAB_ID_KEY = "floorp-workspace-sync-id";
const TAB_OWNER_KEY = "floorp-workspace-sync-owner";
const TAB_PENDING_URL_KEY = "floorp-workspace-sync-url";
const TAB_CONTAINER_REF_KEY = "floorp-workspace-sync-container";
const TAB_CONTAINER_INSTANCE_KEY = "floorp-workspace-sync-container-instance";
const TAB_REOPEN_TARGET_KEY = "floorp-workspace-sync-reopen-target";
const TAB_REOPEN_SOURCE_KEY = "floorp-workspace-sync-reopen-source";
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
  private containerPendingTabs = new Set<string>();
  private containerPendingDefaults = new Set<string>();
  private replacementTabs = new WeakSet<XULElement>();
  private programmaticClosures = new WeakSet<XULElement>();
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
        const tab = event.target as XULElement;
        const programmatic = this.programmaticClosures.has(tab);
        this.programmaticClosures.delete(tab);
        this.incomingNavigations.get(tab)?.finish();
        if (this.active && !programmatic && !this.replacementTabs.has(tab)) {
          const tabId = this.sessionStore.getCustomTabValue(tab, TAB_ID_KEY);
          const workspaceId = tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
          if (workspaceId && isWorkspaceSyncId(tabId)) {
            this.api.noteTabClosed?.(workspaceId, tabId).catch((
              error: unknown,
            ) =>
              console.error(
                "[WorkspacesSync] Could not record explicit tab closure",
                error,
              )
            );
          }
        }
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
        this.sessionStore.setCustomTabValue(tab, TAB_REOPEN_SOURCE_KEY, "");
        this.sessionStore.setCustomTabValue(tab, TAB_REOPEN_TARGET_KEY, "");
        const previous = this.sessionStore.getCustomTabValue(tab, TAB_ID_KEY);
        if (
          this.api.isRetired(previous) && !this.api.isPendingClose(previous)
        ) this.tabId(tab, new Set());
      }
      this.scheduleCapture();
    };
    const onPrefsChange = () => this.scheduleCapture();
    const onWorkspaceTabMoved = (event: Event) =>
      this.onWorkspaceTabMoved(event);
    this.window.addEventListener(
      WORKSPACE_TAB_MOVED_EVENT,
      onWorkspaceTabMoved,
    );
    const onSyncChange = () => this.revision[1]((value) => value + 1);
    const onContainerChange = () => {
      onSyncChange();
      this.scheduleCapture();
      this.window.dispatchEvent(new Event("floorp-workspace-container-ready"));
    };
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
    Services.obs.addObserver(
      onContainerChange,
      WORKSPACE_CONTAINER_BINDINGS_CHANGED,
    );
    Services.prefs.addObserver(WORKSPACE_SYNC_ENGINE_PREF, onSyncChange);

    const registration = new AbortController();
    let unregister: (() => void) | undefined;
    let retryDelay = 1000;
    let retryTimer: number | undefined;
    const initialize = async () => {
      try {
        await this.sessionStore.promiseAllWindowsRestored;
        if (this.stopped) return;
        await this.api.ready();
        await this.recoverContainerReopens();
        if (this.stopped) return;
        unregister = await this.api.register(this, registration.signal);
        if (this.stopped) unregister();
        else {
          this.active = true;
          onSyncChange();
          this.window.dispatchEvent(
            new Event("floorp-workspace-container-ready"),
          );
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
      this.window.removeEventListener(
        WORKSPACE_TAB_MOVED_EVENT,
        onWorkspaceTabMoved,
      );
      Services.obs.removeObserver(onSyncChange, WORKSPACE_SYNC_CHANGED);
      Services.obs.removeObserver(
        onContainerChange,
        WORKSPACE_CONTAINER_BINDINGS_CHANGED,
      );
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

  isContainerAvailable(): boolean {
    this.revision[0]();
    return this.active && (this.api.isContainerSyncEnabled?.() ?? false) &&
      getWorkspaceContainerBindings().isReady();
  }

  defaultContainerPending(id: TWorkspaceID): boolean {
    this.revision[0]();
    if (this.privateWindow) return false;
    if (!this.active) return true;
    if (
      !(this.api.isContainerSyncEnabled?.() ?? false) ||
      !this.api.isSelected(id)
    ) return false;
    const desired = this.api.getSyncedWorkspace(id)?.defaultContainer;
    if (!desired) return false;
    if (!getWorkspaceContainerBindings().isReady()) return true;
    const resolution = getWorkspaceContainerBindings().lookup(desired);
    return resolution.status !== "resolved" ||
      this.containerPendingDefaults.has(id) ||
      Boolean(
        this.api.isContainerDefaultPending(id) &&
          this.ctx.getRawWorkspace(id)?.userContextId !==
            resolution.userContextId,
      );
  }

  containerStatusText(id: TWorkspaceID): string | null {
    this.revision[0]();
    if (!this.isContainerAvailable() || !this.api.isSelected(id)) return null;
    const desired = this.api.getSyncedWorkspace(id);
    if (!desired) return null;
    const bindings = getWorkspaceContainerBindings();
    for (
      const ref of [
        desired.defaultContainer,
        ...desired.tabs.filter((tab) => !tab.deleted).map((tab) =>
          tab.container
        ),
      ]
    ) {
      if (!ref) continue;
      const reason = workspaceContainerPendingText(bindings.lookup(ref));
      if (reason) return reason;
    }
    return this.tabs(id).some((tab) =>
        this.containerPendingTabs.has(this.tabId(tab))
      )
      ? i18next.t("workspaces.containers.reopen-needed")
      : null;
  }

  async setupContainers(id: TWorkspaceID): Promise<void> {
    if (!this.isContainerAvailable() || !this.api.isSelected(id)) return;
    const desired = this.api.getSyncedWorkspace(id);
    if (!desired) return;
    const refs = [
      desired.defaultContainer,
      ...desired.tabs.filter((tab) => !tab.deleted).map((tab) => tab.container),
    ].filter((ref): ref is WorkspaceContainerRef => ref !== undefined);
    try {
      await setupWorkspaceContainers(refs, getWorkspaceContainerBindings(), {
        isCurrent: () =>
          !this.stopped && this.isContainerAvailable() &&
          this.api.isSelected(id),
        unsubscribe: () => this.api.select(id, false),
        retry: () => this.api.capture(),
        capture: () => this.api.capture(),
        resolveLegacy: (target, scope) =>
          this.api.resolveLegacyContainers(id, target, scope),
      });
    } catch (error) {
      console.error("[WorkspacesSync] Container setup failed", error);
      nativeWorkspaceContainerPrompt().alert(
        i18next.t("workspaces.containers.title"),
        i18next.t("workspaces.containers.operation-failed"),
      );
    }
  }

  hasContainerReopen(id: TWorkspaceID): boolean {
    this.revision[0]();
    return this.isContainerAvailable() &&
      this.tabs(id).some((tab) =>
        this.containerPendingTabs.has(this.tabId(tab))
      );
  }

  async reopenContainerTab(id: TWorkspaceID): Promise<void> {
    if (!this.isContainerAvailable() || !this.api.isSelected(id)) return;
    const bindings = getWorkspaceContainerBindings();
    if (!bindings.acquirePrompt()) return;
    const epoch = bindings.getEpoch();
    const prompt = nativeWorkspaceContainerPrompt();
    const title = i18next.t("workspaces.containers.title");
    let source: XULElement | undefined;
    try {
      const pending = this.tabs(id).filter((tab) =>
        this.containerPendingTabs.has(this.tabId(tab))
      );
      const index = prompt.select(
        title,
        i18next.t("workspaces.containers.reopen-select"),
        pending.map((tab) => tab.getAttribute("label") ?? ""),
      );
      if (index === null || !pending[index]) return;
      source = pending[index];
      if (
        !prompt.confirm(
          title,
          i18next.t("workspaces.containers.reopen-confirm"),
        )
      ) return;
    } finally {
      bindings.releasePrompt(epoch);
    }
    if (
      !source || this.stopped || !this.api.isSelected(id) ||
      !bindings.isCurrentEpoch(epoch)
    ) return;
    const tabId = this.tabId(source);
    const desired = this.api.getSyncedTab(id, tabId);
    if (!desired?.container) return;
    const resolution = await bindings.resolve(desired.container);
    if (!bindings.isCurrentEpoch(epoch)) return;
    if (resolution.status !== "resolved") {
      prompt.alert(title, workspaceContainerPendingText(resolution) ?? "");
      return;
    }
    const token = await this.api.beginContainerReopen(id, tabId);
    const transaction = JSON.stringify({ token, workspaceId: id, tabId });
    const abort = new AbortController();
    const cancel = () => abort.abort();
    Services.obs.addObserver(cancel, "weave:service:start-over");
    let replacement: XULElement | undefined;
    let succeeded = false;
    let sourceClosed = false;
    let transferring = false;
    const originalURL = gBrowser.getBrowserForTab(source).currentURI?.spec;
    this.applying = true;
    try {
      if (
        this.stopped || !this.api.isSelected(id) ||
        !bindings.isCurrentEpoch(epoch)
      ) {
        throw new Error("Workspace changed");
      }
      replacement = this.ctx.withExplicitTabUserContext(
        resolution.userContextId,
        () =>
          gBrowser.addTab("about:blank", {
            inBackground: true,
            skipAnimation: true,
            userContextId: resolution.userContextId,
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
          }),
      );
      this.replacementTabs.add(replacement);
      this.sessionStore.setCustomTabValue(
        replacement,
        TAB_REOPEN_TARGET_KEY,
        transaction,
      );
      this.rememberContainer(
        replacement,
        desired.container,
        resolution.instanceId,
      );
      this.sessionStore.setCustomTabValue(
        replacement,
        TAB_PENDING_URL_KEY,
        desired.url,
      );
      await this.waitForIncoming([
        this.navigateIncoming(
          replacement,
          desired.url,
          desired.title,
          abort.signal,
        ),
      ]);
      // Native permitUnload may spin a nested event loop. Complete it before
      // transferring stable identity, then revalidate all participants below.
      if (await this.tabBrowser.runBeforeUnloadForTabs([source])) {
        throw new Error("The original tab's unload was canceled");
      }
      const latest = this.api.getSyncedTab(id, tabId);
      const current = bindings.lookup(desired.container);
      if (
        abort.signal.aborted || this.stopped || !this.api.isSelected(id) ||
        !bindings.isCurrentEpoch(epoch) ||
        this.api.isRetired(tabId) || latest?.url !== desired.url ||
        JSON.stringify(latest?.container) !==
          JSON.stringify(desired.container) ||
        current.status !== "resolved" ||
        current.userContextId !== resolution.userContextId ||
        current.instanceId !== resolution.instanceId ||
        ![...gBrowser.tabs].includes(source) ||
        gBrowser.getBrowserForTab(source).currentURI?.spec !== originalURL
      ) throw new Error("Workspace container changed during tab replacement");
      if (desired.pinned) gBrowser.pinTab(replacement);
      gBrowser.moveTabTo(replacement, {
        tabIndex: [...gBrowser.tabs].indexOf(source),
      });
      // Keep exactly one owner while changing the physical browsing context.
      transferring = true;
      this.sessionStore.setCustomTabValue(
        source,
        TAB_REOPEN_SOURCE_KEY,
        transaction,
      );
      this.sessionStore.setCustomTabValue(source, TAB_ID_KEY, "");
      this.sessionStore.setCustomTabValue(source, TAB_OWNER_KEY, "");
      this.ctx.tabManagerCtx.setWorkspaceIdToAttribute(replacement, id);
      this.sessionStore.setCustomTabValue(replacement, TAB_ID_KEY, tabId);
      this.sessionStore.setCustomTabValue(replacement, TAB_OWNER_KEY, id);
      this.rememberContainer(
        replacement,
        desired.container,
        resolution.instanceId,
      );
      if (gBrowser.selectedTab === source) gBrowser.selectedTab = replacement;
      this.removeProgrammaticTab(source, true, true);
      if ([...gBrowser.tabs].includes(source)) {
        this.sessionStore.setCustomTabValue(replacement, TAB_ID_KEY, "");
        this.sessionStore.setCustomTabValue(replacement, TAB_OWNER_KEY, "");
        this.sessionStore.setCustomTabValue(source, TAB_ID_KEY, tabId);
        this.sessionStore.setCustomTabValue(source, TAB_OWNER_KEY, id);
        throw new Error("The original tab was not closed");
      }
      sourceClosed = true;
      this.replacementTabs.delete(replacement);
      this.sessionStore.setCustomTabValue(
        replacement,
        TAB_REOPEN_TARGET_KEY,
        "",
      );
      this.containerPendingTabs.delete(tabId);
      succeeded = true;
    } catch (error) {
      abort.abort();
      if (transferring && [...gBrowser.tabs].includes(source)) {
        if (replacement) {
          this.sessionStore.setCustomTabValue(replacement, TAB_ID_KEY, "");
          this.sessionStore.setCustomTabValue(replacement, TAB_OWNER_KEY, "");
        }
        this.sessionStore.setCustomTabValue(source, TAB_ID_KEY, tabId);
        this.sessionStore.setCustomTabValue(source, TAB_OWNER_KEY, id);
        this.sessionStore.setCustomTabValue(source, TAB_REOPEN_SOURCE_KEY, "");
      }
      if (replacement && !succeeded && !sourceClosed) {
        this.removeProgrammaticTab(replacement, false, true);
      }
      console.error("[WorkspacesSync] Could not reopen container tab", error);
      prompt.alert(title, i18next.t("workspaces.containers.operation-failed"));
    } finally {
      Services.obs.removeObserver(cancel, "weave:service:start-over");
      this.applying = false;
      await this.api.finishContainerReopen(token, succeeded);
      this.ctx.tabManagerCtx.updateTabsVisibility();
      this.revision[1]((value) => value + 1);
      this.scheduleCapture();
    }
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
    return code && code !== "container-pending"
      ? i18next.t(`workspaces.sync.${code}`)
      : null;
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

  private onWorkspaceTabMoved(event: Event): void {
    if (this.privateWindow || this.stopped) return;
    const detail = (event as CustomEvent<WorkspaceTabMoveDetail>).detail;
    if (!detail?.tab || !isWorkspaceSyncId(detail.fromWorkspaceId)) return;
    const tabId = this.sessionStore.getCustomTabValue(detail.tab, TAB_ID_KEY);
    if (!isWorkspaceSyncId(tabId)) return;
    this.api.noteTabClosed(detail.fromWorkspaceId, tabId, "move").then(
      () => this.scheduleCapture(),
      (error: unknown) =>
        console.error(
          "[WorkspacesSync] Could not record explicit tab move",
          error,
        ),
    );
  }

  private tabs(id: string): XULElement[] {
    return [...gBrowser.tabs].filter((tab) =>
      !this.replacementTabs.has(tab) &&
      !this.sessionStore.getCustomTabValue(tab, TAB_REOPEN_TARGET_KEY) &&
      tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID) === id
    );
  }

  private removeProgrammaticTab(
    tab: XULElement,
    skipPermitUnload = false,
    skipSessionStore = false,
  ): void {
    this.programmaticClosures.add(tab);
    try {
      this.tabBrowser.removeTab(tab, {
        animate: false,
        skipPermitUnload,
        skipSessionStore,
      });
    } finally {
      // A canceled close must not suppress the user's next ordinary close.
      if ([...gBrowser.tabs].includes(tab) && !tab.hasAttribute("closing")) {
        this.programmaticClosures.delete(tab);
      }
    }
  }

  private reopenState(
    tab: XULElement,
    key: string,
  ): WorkspaceContainerReopenState | null {
    try {
      const value = JSON.parse(
        this.sessionStore.getCustomTabValue(tab, key),
      ) as Partial<WorkspaceContainerReopenState>;
      return typeof value.token === "string" && value.token &&
          isWorkspaceSyncId(value.workspaceId) && isWorkspaceSyncId(value.tabId)
        ? value as WorkspaceContainerReopenState
        : null;
    } catch {
      return null;
    }
  }

  private async recoverContainerReopens(): Promise<void> {
    const handled = new Set<string>();
    for (const target of [...gBrowser.tabs]) {
      const transaction = this.reopenState(target, TAB_REOPEN_TARGET_KEY);
      if (!transaction) continue;
      const source = this.allBrowserTabs().find((tab) =>
        tab !== target && (
          this.reopenState(tab, TAB_REOPEN_SOURCE_KEY)?.token ===
                transaction.token &&
            (!isWorkspaceSyncId(
              this.sessionStore.getCustomTabValue(tab, TAB_ID_KEY),
            ) ||
              this.sessionStore.getCustomTabValue(tab, TAB_ID_KEY) ===
                transaction.tabId) ||
          this.sessionStore.getCustomTabValue(tab, TAB_ID_KEY) ===
                transaction.tabId &&
            this.sessionStore.getCustomTabValue(tab, TAB_OWNER_KEY) ===
              transaction.workspaceId
        )
      );
      const committed = !source &&
        this.sessionStore.getCustomTabValue(target, TAB_ID_KEY) ===
          transaction.tabId;
      if (source) {
        this.sessionStore.setCustomTabValue(
          source,
          TAB_ID_KEY,
          transaction.tabId,
        );
        this.sessionStore.setCustomTabValue(
          source,
          TAB_OWNER_KEY,
          transaction.workspaceId,
        );
        this.sessionStore.setCustomTabValue(source, TAB_REOPEN_SOURCE_KEY, "");
      }
      if (committed) {
        this.sessionStore.setCustomTabValue(target, TAB_REOPEN_TARGET_KEY, "");
      } else this.removeProgrammaticTab(target, false, true);
      handled.add(transaction.token);
      try {
        await this.api.finishContainerReopen(transaction.token, committed);
      } catch (error) {
        console.error(
          "[WorkspacesSync] Could not finish restored container operation",
          error,
        );
      }
    }
    for (const source of [...gBrowser.tabs]) {
      const transaction = this.reopenState(source, TAB_REOPEN_SOURCE_KEY);
      if (!transaction || handled.has(transaction.token)) continue;
      const currentId = this.sessionStore.getCustomTabValue(source, TAB_ID_KEY);
      const ownedElsewhere = this.allBrowserTabs().some((tab) =>
        tab !== source &&
        this.sessionStore.getCustomTabValue(tab, TAB_ID_KEY) ===
          transaction.tabId
      );
      if (
        !ownedElsewhere && !this.api.isRetired(transaction.tabId) &&
        (!isWorkspaceSyncId(currentId) || currentId === transaction.tabId)
      ) {
        this.sessionStore.setCustomTabValue(
          source,
          TAB_ID_KEY,
          transaction.tabId,
        );
        this.sessionStore.setCustomTabValue(
          source,
          TAB_OWNER_KEY,
          transaction.workspaceId,
        );
      }
      this.sessionStore.setCustomTabValue(source, TAB_REOPEN_SOURCE_KEY, "");
      try {
        await this.api.finishContainerReopen(transaction.token, false);
      } catch (error) {
        console.error(
          "[WorkspacesSync] Could not restore container source tab",
          error,
        );
      }
    }
  }

  private allBrowserTabs(): XULElement[] {
    const tabs: XULElement[] = [];
    for (const candidate of Services.wm.getEnumerator("navigator:browser")) {
      const browserWindow = candidate as unknown as {
        gBrowser?: { tabs: XULElement[] };
      };
      tabs.push(...(browserWindow.gBrowser?.tabs ?? []));
    }
    return tabs;
  }

  private localContainerId(tab: XULElement): number {
    const nativeId = this.tabBrowser.getBrowserForTab(tab).browsingContext
      ?.originAttributes.userContextId;
    if (
      typeof nativeId === "number" && Number.isSafeInteger(nativeId) &&
      nativeId >= 0
    ) return nativeId;
    return Number.parseInt(tab.getAttribute("usercontextid") || "0", 10);
  }

  private savedContainerRef(
    tab: XULElement,
  ): WorkspaceContainerRef | undefined {
    const value = this.sessionStore.getCustomTabValue(
      tab,
      TAB_CONTAINER_REF_KEY,
    );
    if (!value) return undefined;
    try {
      return parseWorkspaceContainerRef(JSON.parse(value));
    } catch {
      return undefined;
    }
  }

  private rememberContainer(
    tab: XULElement,
    ref: WorkspaceContainerRef,
    instanceId?: string,
  ): void {
    this.sessionStore.setCustomTabValue(
      tab,
      TAB_CONTAINER_REF_KEY,
      JSON.stringify(ref),
    );
    this.sessionStore.setCustomTabValue(
      tab,
      TAB_CONTAINER_INSTANCE_KEY,
      instanceId ?? "",
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

  containerTabIds(userContextIds: readonly number[]): Record<string, string[]> {
    const requested = new Set(userContextIds);
    const grouped: Record<string, string[]> = {};
    for (const tab of gBrowser.tabs) {
      if (
        !requested.has(this.localContainerId(tab)) ||
        this.replacementTabs.has(tab)
      ) continue;
      const id = this.sessionStore.getCustomTabValue(tab, TAB_ID_KEY);
      const owner = tab.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
      if (!isWorkspaceSyncId(id) || !isWorkspaceSyncId(owner)) continue;
      (grouped[owner] ??= []).push(id);
    }
    return grouped;
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
    const containersEnabled = this.api.isContainerSyncEnabled?.() ?? false;
    const bindings = containersEnabled ? getWorkspaceContainerBindings() : null;
    const desired = containersEnabled ? this.api.getSyncedWorkspace(id) : null;
    const protectedTabIds: string[] = [];
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
      if (bindings && known?.container) {
        const resolved = bindings.lookup(known.container);
        if (
          workspaceContainerNeedsReopen(
            this.localContainerId(tab),
            this.sessionStore.getCustomTabValue(
              tab,
              TAB_CONTAINER_INSTANCE_KEY,
            ),
            resolved,
          )
        ) {
          // Keep the desired remote record while the real tab belongs to an
          // unresolved or different container. Never restamp its old account.
          if (this.tabId(tab, seenIds) === known.id) {
            tabs.push(known);
            protectedTabIds.push(known.id);
            this.containerPendingTabs.add(known.id);
          }
          continue;
        }
      }
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
      const container = bindings?.captureReference(
        this.localContainerId(tab),
        this.savedContainerRef(tab) ?? known?.container,
      ).ref;
      if (container && bindings) {
        const resolved = bindings.lookup(container);
        this.rememberContainer(
          tab,
          container,
          resolved.status === "resolved" ? resolved.instanceId : undefined,
        );
      }
      tabs.push({
        id: this.tabId(tab, seenIds),
        url,
        title: this.incomingNavigations.get(tab)?.title ??
          (isSyncableWorkspaceURL(browser.currentURI?.spec)
            ? (tab.getAttribute("label") ?? "").slice(0, 1024).toWellFormed()
            : known?.title ?? ""),
        pinned: tab.hasAttribute("pinned"),
        ...(container ? { container } : {}),
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
    let defaultContainer: WorkspaceContainerRef | undefined;
    if (bindings) {
      const desiredDefault = desired?.defaultContainer;
      defaultContainer = desiredDefault &&
          (this.containerPendingDefaults.has(id) ||
            this.api.isContainerDefaultPending(id) ||
            bindings.lookup(desiredDefault).status !== "resolved")
        ? desiredDefault
        : bindings.captureReference(
          workspace.userContextId ?? 0,
          desiredDefault,
        ).ref;
    }
    return {
      ...(containersEnabled
        ? { version: 2, defaultContainer, protectedTabIds }
        : {}),
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
  ): Promise<void | WorkspaceSyncApplyResult> {
    if (signal?.aborted) {
      throw new Error("Workspace Sync account changed");
    }
    if (this.stopped) throw new Error("Workspace window is shutting down");
    const navigations: IncomingWorkspaceNavigation[] = [];
    const pendingContainerTabIds: string[] = [];
    let pendingDefaultContainer = false;
    let completed = false;
    this.applying = true;
    this.window.clearTimeout(this.timer);
    try {
      const id = document.workspaceId as TWorkspaceID;
      let localContainerId = this.ctx.getRawWorkspace(id)?.userContextId ?? 0;
      const bindings = document.version === 2
        ? getWorkspaceContainerBindings()
        : null;
      if (bindings) {
        if (!document.defaultContainer) {
          throw new Error("Missing workspace container reference");
        }
        const resolution = await bindings.resolve(document.defaultContainer);
        if (signal?.aborted || this.stopped) {
          throw new Error("Workspace Sync account changed");
        }
        pendingDefaultContainer = resolution.status !== "resolved";
        if (resolution.status === "resolved") {
          localContainerId = resolution.userContextId;
          this.containerPendingDefaults.delete(id);
        } else this.containerPendingDefaults.add(id);
      }
      setWorkspacesDataStore("data", (previous) => {
        const map = new Map(
          previous as unknown as Map<TWorkspaceID, TWorkspace>,
        );
        const existing = map.get(id);
        map.set(id, {
          ...existing,
          name: document.name,
          icon: document.icon,
          userContextId: localContainerId,
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
          this.removeProgrammaticTab(tab);
        }
      }
      let previousTab: XULElement | undefined;
      for (const remoteTab of liveWorkspaceSyncTabs(document)) {
        let tab = local.get(remoteTab.id);
        let created = false;
        let tabContainerId = localContainerId;
        let instanceId: string | undefined;
        if (bindings) {
          if (!remoteTab.container) {
            throw new Error("Missing tab container reference");
          }
          const resolution = await bindings.resolve(remoteTab.container);
          if (signal?.aborted || this.stopped) {
            throw new Error("Workspace Sync account changed");
          }
          if (
            resolution.status !== "resolved" ||
            (tab && workspaceContainerNeedsReopen(
              this.localContainerId(tab),
              this.sessionStore.getCustomTabValue(
                tab,
                TAB_CONTAINER_INSTANCE_KEY,
              ),
              resolution,
            ))
          ) {
            pendingContainerTabIds.push(remoteTab.id);
            this.containerPendingTabs.add(remoteTab.id);
            this.incomingNavigations.get(tab as XULElement)?.cancel(
              new Error("Workspace container requires setup"),
            );
            if (tab) previousTab = tab;
            continue;
          }
          tabContainerId = resolution.userContextId;
          instanceId = resolution.instanceId;
          this.containerPendingTabs.delete(remoteTab.id);
        }
        if (!tab && missingTabIds.has(remoteTab.id)) {
          tab = this.ctx.withExplicitTabUserContext(
            tabContainerId,
            () =>
              gBrowser.addTab(remoteTab.url, {
                inBackground: true,
                skipAnimation: true,
                userContextId: tabContainerId,
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
        if (remoteTab.container) {
          this.rememberContainer(tab, remoteTab.container, instanceId);
        }
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
      if (bindings) {
        this.revision[1]((value) => value + 1);
        this.window.dispatchEvent(
          new Event("floorp-workspace-container-ready"),
        );
        return { pendingContainerTabIds, pendingDefaultContainer };
      }
    } finally {
      this.applying = false;
      if (
        completed && !pendingDefaultContainer &&
        pendingContainerTabIds.length === 0
      ) this.scheduleCapture();
    }
  }
}
