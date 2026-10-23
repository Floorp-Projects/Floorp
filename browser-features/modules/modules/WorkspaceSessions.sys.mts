// SPDX-License-Identifier: MPL-2.0

const { ContextualIdentityService } = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
);
const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
);

const WORKSPACE_PREF = "floorp.workspaces.v4.store";
export const PENDING_SESSION_KEY = "floorp.workspace.pendingSession";
export const PENDING_SESSION_ATTRIBUTE = "floorpPendingSessionState";
export const PENDING_SESSION_ID_ATTRIBUTE = "floorpPendingSessionId";

interface SessionTab extends Element {
  userContextId?: number;
  pinned?: boolean;
  isConnected: boolean;
  linkedBrowser?: {
    currentURI?: { spec: string };
  };
}

interface SessionWindow extends Window {
  workspacesFuncs?: {
    getSelectedWorkspaceID?(): string;
  };
  gBrowser?: {
    selectedTab: SessionTab;
    tabs: Iterable<SessionTab>;
    addTab(url: string, options: Record<string, unknown>): SessionTab;
    removeTab(tab: SessionTab, options?: Record<string, unknown>): void;
    pinTab(tab: SessionTab): void;
    hideTab(tab: SessionTab): void;
  };
}

interface SessionTabState extends Record<string, unknown> {
  userContextId?: number;
  entries?: Array<{ url?: string; title?: string }>;
  index?: number;
  attributes?: Record<string, unknown>;
  extData?: Record<string, string>;
  floorpWorkspaceId?: string;
}

interface NewTabSessionOptions {
  floorpWorkspaceId?: string | null;
  userContextId?: unknown;
  openerTab?: SessionTab | null;
  openerBrowser?: Element | null;
  openWindowInfo?: { originAttributes?: { userContextId?: number } } | null;
  uriString?: string | null;
  skipLoad?: boolean;
}

interface NewTabSessionResult {
  workspaceId: string | null;
  userContextId: number;
  uriString: string | null | undefined;
  pending: SessionTabState | null;
}

function workspaceStore(): {
  defaultID?: string;
  data?:
    | Array<[string, { userContextId?: number }]>
    | Record<string, {
      userContextId?: number;
    }>;
} {
  try {
    return JSON.parse(Services.prefs.getStringPref(WORKSPACE_PREF, "{}"));
  } catch {
    throw new Error("Workspace session settings could not be read");
  }
}

function checkedContext(value: unknown): number {
  if (typeof value !== "number" || !Number.isSafeInteger(value) || value < 0) {
    throw new RangeError("Invalid workspace session ID");
  }
  return value;
}

function browserOriginContext(rawBrowser: unknown): number | undefined {
  const browser = rawBrowser as {
    browsingContext?: { originAttributes?: { userContextId?: unknown } };
  } | null;
  const actual = browser?.browsingContext?.originAttributes?.userContextId;
  return actual === undefined ? undefined : checkedContext(actual);
}

function currentURL(state: SessionTabState): string {
  const entries = state.entries ?? [];
  const index = Math.min(
    Math.max((state.index ?? entries.length) - 1, 0),
    entries.length - 1,
  );
  return entries[index]?.url ?? "about:blank";
}

function sessionStore() {
  return ChromeUtils.importESModule(
    "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
  ).SessionStore as {
    getCustomTabValue(tab: SessionTab, key: string): string;
    setCustomTabValue(tab: SessionTab, key: string, value: string): void;
    deleteCustomTabValue(tab: SessionTab, key: string): void;
  };
}

export const WorkspaceSessions = {
  isPrivateWindow(window: unknown): boolean {
    return PrivateBrowsingUtils.isWindowPrivate(window);
  },
  isValidContext(value: unknown): value is number {
    if (
      typeof value !== "number" || !Number.isSafeInteger(value) || value < 0
    ) return false;
    if (value === 0) return true;
    ContextualIdentityService.ensureDataReady();
    // Selection lists exclude Floorp private containers; native routing must
    // still preserve every existing origin context, including Gecko's private IDs.
    return Boolean(ContextualIdentityService.getPublicIdentityFromId(value)) ||
      ContextualIdentityService.getPrivateUserContextIds().includes(value);
  },

  getWindowWorkspaceId(rawWindow: unknown): string | null {
    const window = rawWindow as SessionWindow;
    if (!Services.prefs.getBoolPref("floorp.workspaces.enabled", true)) {
      return null;
    }
    const selected = window.workspacesFuncs?.getSelectedWorkspaceID?.() ??
      window.gBrowser?.selectedTab?.getAttribute("floorpWorkspaceId");
    return selected || workspaceStore().defaultID || null;
  },

  getDefaultContext(rawWindow: unknown): number {
    if (!Services.prefs.getBoolPref("floorp.workspaces.enabled", true)) {
      return 0;
    }
    const window = rawWindow as SessionWindow;
    if (this.isPrivateWindow(window)) return 0;
    const store = workspaceStore();
    const id = this.getWindowWorkspaceId(window);
    const definitions = Array.isArray(store.data)
      ? store.data
      : Object.entries(store.data ?? {});
    const workspace = new Map(definitions).get(id ?? "");
    return checkedContext(workspace?.userContextId ?? 0);
  },

  resolveNewTab(
    rawWindow: unknown,
    options: NewTabSessionOptions,
  ): NewTabSessionResult {
    const window = rawWindow as SessionWindow;
    const workspaceId =
      Services.prefs.getBoolPref("floorp.workspaces.enabled", true) &&
        !this.isPrivateWindow(window)
        ? options.floorpWorkspaceId || this.getWindowWorkspaceId(window)
        : null;
    const explicit = options.userContextId;
    const origin = options.openWindowInfo?.originAttributes?.userContextId ??
      (options.openerBrowser
        ? browserOriginContext(options.openerBrowser) ??
          Number(options.openerBrowser.getAttribute("usercontextid") || 0)
        : options.openerTab
        ? browserOriginContext(options.openerTab.linkedBrowser) ??
          Number(options.openerTab.getAttribute("usercontextid") || 0)
        : undefined);
    const context = explicit !== undefined && explicit !== null
      ? checkedContext(explicit)
      : origin !== undefined
      ? checkedContext(origin)
      : options.skipLoad
      ? 0
      : this.getDefaultContext(window);
    if (this.isValidContext(context)) {
      return {
        workspaceId,
        userContextId: context,
        uriString: options.uriString,
        pending: null,
      };
    }
    const pending: SessionTabState = {
      entries: [{ url: options.uriString || "about:blank" }],
      index: 1,
      userContextId: context,
      floorpWorkspaceId: workspaceId ?? undefined,
    };
    return { workspaceId, userContextId: 0, uriString: "about:blank", pending };
  },

  sanitizeRestoreState(rawState: unknown): SessionTabState {
    if (!rawState || typeof rawState !== "object") {
      throw new TypeError("Invalid tab state");
    }
    const state = rawState as SessionTabState;
    const saved = state.extData?.[PENDING_SESSION_KEY];
    const original = saved ? JSON.parse(saved) as SessionTabState : state;
    if (!saved && this.isValidContext(state.userContextId ?? 0)) return state;
    const workspaceId = original.floorpWorkspaceId ??
      original.attributes?.floorpWorkspaceId;
    const attributes: Record<string, unknown> = {
      floorpPendingSessionId: String(original.userContextId ?? 0),
    };
    if (workspaceId) attributes.floorpWorkspaceId = workspaceId;
    const lastShown = original.floorpLastShowWorkspaceId ??
      original.attributes?.floorpWorkspaceLastShowId;
    if (lastShown) attributes.floorpWorkspaceLastShowId = lastShown;
    const placeholder: SessionTabState = {
      entries: [{ url: "about:blank" }],
      index: 1,
      userContextId: 0,
      attributes,
      extData: { [PENDING_SESSION_KEY]: saved ?? JSON.stringify(original) },
      floorpWorkspaceId: typeof workspaceId === "string"
        ? workspaceId
        : undefined,
    };
    for (
      const key of [
        "pinned",
        "hidden",
        "lastAccessed",
        "groupId",
        "splitViewId",
        "floorpLastShowWorkspaceId",
      ]
    ) {
      if (key in state) placeholder[key] = state[key];
    }
    return placeholder;
  },

  markPendingTab(rawTab: unknown, rawState: unknown): void {
    const tab = rawTab as SessionTab;
    const state = rawState as SessionTabState;
    const saved = state.extData?.[PENDING_SESSION_KEY] ?? JSON.stringify(state);
    const original = JSON.parse(saved) as SessionTabState;
    tab.setAttribute(
      PENDING_SESSION_ID_ATTRIBUTE,
      String(original.userContextId ?? 0),
    );
    tab.setAttribute(PENDING_SESSION_ATTRIBUTE, saved);
    const workspaceId = original.floorpWorkspaceId ??
      original.attributes?.floorpWorkspaceId;
    if (typeof workspaceId === "string") {
      tab.setAttribute("floorpWorkspaceId", workspaceId);
    }
    try {
      sessionStore().setCustomTabValue(tab, PENDING_SESSION_KEY, saved);
    } catch (error) {
      console.error(
        "[WorkspaceSessions] Pending state is retained on the tab",
        error,
      );
    }
  },

  getPendingState(rawTab: unknown): SessionTabState | null {
    const tab = rawTab as SessionTab;
    let saved = tab.getAttribute(PENDING_SESSION_ATTRIBUTE);
    if (!saved) {
      try {
        saved = sessionStore().getCustomTabValue(tab, PENDING_SESSION_KEY);
      } catch {
        return null;
      }
    }
    if (!saved) return null;
    try {
      return JSON.parse(saved) as SessionTabState;
    } catch {
      return null;
    }
  },

  canReopenURL(url: string): boolean {
    return /^(https?|about|file):/i.test(url);
  },

  pendingTabs(
    workspaceId: string,
    oldId: number,
  ): Array<{ window: SessionWindow; tab: SessionTab; state: SessionTabState }> {
    const found: Array<
      { window: SessionWindow; tab: SessionTab; state: SessionTabState }
    > = [];
    for (const rawWindow of Services.wm.getEnumerator("navigator:browser")) {
      const window = rawWindow as unknown as SessionWindow;
      if (this.isPrivateWindow(window)) continue;
      for (const tab of window.gBrowser?.tabs ?? []) {
        if (tab.getAttribute("floorpWorkspaceId") !== workspaceId) continue;
        const state = this.getPendingState(tab);
        if (state?.userContextId === oldId) found.push({ window, tab, state });
      }
    }
    return found;
  },

  recoverPendingTabs(
    workspaceId: string,
    oldId: number,
    newId: number,
  ): { recovered: number; failed: number } {
    if (!this.isValidContext(newId)) {
      throw new Error("Recovery session is missing");
    }
    const pending = this.pendingTabs(workspaceId, oldId);
    if (pending.some(({ state }) => !this.canReopenURL(currentURL(state)))) {
      throw new Error("A pending URL cannot be reopened in another session");
    }
    const replacements: Array<{
      window: SessionWindow;
      original: SessionTab;
      replacement: SessionTab;
      selected: boolean;
    }> = [];
    // Prepare every new browser before removing a placeholder. If creation
    // fails, the complete original restore data remains available for retry.
    for (const { window, tab, state } of pending) {
      const browser = window.gBrowser;
      try {
        if (!browser || window.closed || !tab.isConnected) {
          throw new Error("A pending recovery window or tab was closed");
        }
        const selected = browser.selectedTab === tab;
        const replacement = browser.addTab(currentURL(state), {
          userContextId: newId,
          floorpWorkspaceId: workspaceId,
          inBackground: true,
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
        });
        if (!replacement) throw new Error("Recovery tab could not be created");
        replacements.push({ window, original: tab, replacement, selected });
        replacement.setAttribute("floorpWorkspaceId", workspaceId);
        if (tab.hasAttribute("floorpWorkspaceLastShowId")) {
          replacement.setAttribute("floorpWorkspaceLastShowId", workspaceId);
        }
        if (tab.pinned) browser.pinTab(replacement);
        if (this.getWindowWorkspaceId(window) !== workspaceId && !selected) {
          browser.hideTab(replacement);
        }
      } catch (error) {
        for (const prepared of replacements) {
          prepared.window.gBrowser?.removeTab(prepared.replacement, {
            animate: false,
            skipSessionStore: true,
          });
        }
        console.error("[WorkspaceSessions] Pending tab recovery failed", error);
        return { recovered: 0, failed: pending.length };
      }
    }
    for (const { window, original, replacement, selected } of replacements) {
      const browser = window.gBrowser!;
      if (selected) browser.selectedTab = replacement;
      browser.removeTab(original, { animate: false, skipSessionStore: true });
    }
    return { recovered: replacements.length, failed: 0 };
  },
};
