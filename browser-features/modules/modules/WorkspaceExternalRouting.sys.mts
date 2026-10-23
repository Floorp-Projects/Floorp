// SPDX-License-Identifier: MPL-2.0

import type {
  ExternalOpenOptions,
  InitialWindowOptions,
  InitialWindowSession,
  RoutingTab,
  RoutingWindow,
  WorkspaceExternalRequest,
} from "./WorkspaceExternalRouting.types.mts";

const { WorkspaceSessions } = ChromeUtils.importESModule(
  "resource://noraneko/modules/WorkspaceSessions.sys.mjs",
) as {
  WorkspaceSessions:
    typeof import("./WorkspaceSessions.sys.mts").WorkspaceSessions;
};

const requests = new WeakMap<RoutingWindow, WorkspaceExternalRequest>();
const initialSessions = new WeakMap<RoutingWindow, InitialWindowSession>();
const WORKSPACE_ATTRIBUTE = "floorpWorkspaceId";
let cancelledRequestId = 0;

function defaultWorkspace(): { id: string | null; userContextId: number } {
  if (!Services.prefs.getBoolPref("floorp.workspaces.enabled", true)) {
    return { id: null, userContextId: 0 };
  }
  const store = JSON.parse(
    Services.prefs.getStringPref("floorp.workspaces.v4.store", "{}"),
  ) as {
    defaultID?: string;
    data?:
      | Array<[string, { userContextId?: number }]>
      | Record<string, { userContextId?: number }>;
  };
  const definitions = new Map(
    Array.isArray(store.data) ? store.data : Object.entries(store.data ?? {}),
  );
  const id = store.defaultID && definitions.has(store.defaultID)
    ? store.defaultID
    : null;
  const userContextId = id ? definitions.get(id)?.userContextId ?? 0 : 0;
  if (!Number.isSafeInteger(userContextId) || userContextId < 0) {
    throw new RangeError("Invalid default workspace session");
  }
  return { id, userContextId };
}

function workspaceExists(id: string): boolean {
  const store = JSON.parse(
    Services.prefs.getStringPref("floorp.workspaces.v4.store", "{}"),
  ) as {
    data?: Array<[string, unknown]> | Record<string, unknown>;
  };
  return Array.isArray(store.data)
    ? store.data.some(([workspaceId]) => workspaceId === id)
    : Object.hasOwn(store.data ?? {}, id);
}

function translated(key: string, fallback: string): string {
  const { I18nUtils } = ChromeUtils.importESModule(
    "resource://noraneko/modules/i18n/I18n-Utils.sys.mjs",
  ) as {
    I18nUtils: {
      getTranslationProvider(): {
        t?(key: string, options: Record<string, unknown>): unknown;
      } | null;
    };
  };
  const value = I18nUtils.getTranslationProvider()?.t?.(key, {
    defaultValue: fallback,
  });
  return typeof value === "string" ? value : fallback;
}

function actualContext(tab: RoutingTab): number {
  return tab.linkedBrowser.browsingContext?.originAttributes?.userContextId ??
    Number(tab.getAttribute("usercontextid") || 0);
}

/** Snapshot at the external entry point; never guess context from a URL. */
function capture(
  rawWindow: unknown = null,
  isPrivate = false,
): WorkspaceExternalRequest {
  const window = rawWindow as RoutingWindow | null;
  if (!window) {
    const workspace = isPrivate
      ? { id: null, userContextId: 0 }
      : defaultWorkspace();
    return Object.freeze({
      window: null,
      workspaceId: workspace.id,
      userContextId: workspace.userContextId,
      reuseTab: null,
    });
  }
  const supplied = requests.get(window);
  if (supplied) return supplied;
  return Object.freeze({
    window,
    workspaceId: WorkspaceSessions.isPrivateWindow(window)
      ? null
      : WorkspaceSessions.getWindowWorkspaceId(window),
    userContextId: WorkspaceSessions.getDefaultContext(window),
    reuseTab: window.gBrowser?.selectedTab ?? null,
  });
}

function windowWorkspace(window: RoutingWindow): string | null {
  return WorkspaceSessions.isPrivateWindow(window)
    ? null
    : WorkspaceSessions.getWindowWorkspaceId(window);
}

function isCurrent(
  window: RoutingWindow,
  request: WorkspaceExternalRequest,
): boolean {
  return !window.closed && windowWorkspace(window) === request.workspaceId &&
    (!request.reuseTab || window.gBrowser?.selectedTab === request.reuseTab);
}

export const WorkspaceExternalRouting = {
  capture,

  /** Preserve a captured request across deferred BrowserContentHandler delivery. */
  withRequest<T>(
    rawWindow: unknown,
    request: WorkspaceExternalRequest,
    open: () => T,
  ): T {
    const window = rawWindow as RoutingWindow;
    if (request.window && request.window !== window) {
      throw new Error("External workspace request belongs to another window");
    }
    const previous = requests.get(window);
    const bound = Object.freeze({ ...request, window });
    requests.set(window, bound);
    try {
      return open();
    } finally {
      if (previous) requests.set(window, previous);
      else requests.delete(window);
    }
  },

  canReuse(rawWindow: unknown, request: WorkspaceExternalRequest): boolean {
    const window = rawWindow as RoutingWindow;
    const tab = request.reuseTab;
    return request.window === window && Boolean(tab?.isConnected) &&
      window.gBrowser?.selectedTab === tab && isCurrent(window, request) &&
      (request.workspaceId === null ||
        tab?.getAttribute(WORKSPACE_ATTRIBUTE) === request.workspaceId) &&
      tab !== null && actualContext(tab) === request.userContextId &&
      !tab.hasAttribute("pending") && !WorkspaceSessions.getPendingState(tab);
  },

  /** Return the browser's context just like nsIBrowserDOMWindow.openURI. */
  open(
    rawWindow: unknown,
    uri: { spec: string } | null,
    options: ExternalOpenOptions,
  ): unknown {
    const window = rawWindow as RoutingWindow;
    const browser = window.gBrowser;
    if (!browser || window.closed) return null;
    const request = options.request ?? capture(window);
    if (request.window && request.window !== window) return null;
    if (request.workspaceId && !workspaceExists(request.workspaceId)) {
      const box = window.gNotificationBox;
      if (box) {
        box.appendNotification(
          `floorp-workspace-external-cancelled-${++cancelledRequestId}`,
          {
            label: translated(
              "workspaceRouting.removedTarget",
              "The destination workspace was removed. This link has not been opened.",
            ),
            priority: box.PRIORITY_WARNING_MEDIUM,
          },
          [{
            label: translated(
              "workspaceRouting.openHere",
              "Open in the current workspace",
            ),
            callback: () => {
              this.open(window, uri, { ...options, request: capture(window) });
            },
          }],
        ).catch(console.error);
      }
      return null;
    }
    const current = isCurrent(window, request);
    const url = !uri || uri.spec === "about:blank"
      ? (window as RoutingWindow & { BROWSER_NEW_TAB_URL?: string })
        .BROWSER_NEW_TAB_URL ?? "about:newtab"
      : uri.spec;
    const loadFlags = (Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL ?? 0) |
      (options.forceAllowDataURI
        ? (Ci.nsIWebNavigation.LOAD_FLAGS_FORCE_ALLOW_DATA_URI ?? 0)
        : 0);
    if (
      options.where === Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW &&
      this.canReuse(window, request)
    ) {
      const target = request.reuseTab!.linkedBrowser;
      target.fixupAndLoadURIString(url, {
        triggeringPrincipal: options.triggeringPrincipal,
        policyContainer: options.policyContainer,
        referrerInfo: options.referrerInfo,
        loadFlags,
      });
      if (
        !Services.prefs.getBoolPref(
          "browser.tabs.loadDivertedInBackground",
          false,
        )
      ) window.focus();
      return target.browsingContext;
    }
    const inBackground = !current ||
      options.where === Ci.nsIBrowserDOMWindow.OPEN_NEWTAB_BACKGROUND ||
      (options.where !== Ci.nsIBrowserDOMWindow.OPEN_NEWTAB_FOREGROUND &&
        Services.prefs.getBoolPref(
          "browser.tabs.loadDivertedInBackground",
          false,
        ));
    const tabOptions = {
      triggeringPrincipal: options.triggeringPrincipal,
      policyContainer: options.policyContainer,
      referrerInfo: options.referrerInfo,
      userContextId: request.userContextId,
      floorpWorkspaceId: request.workspaceId,
      fromExternal: true,
      inBackground,
      forceAllowDataURI: options.forceAllowDataURI,
    };
    const tab = options.where ===
          Reflect.get(Ci.nsIBrowserDOMWindow, "OPEN_NEWTAB_AFTER_CURRENT") &&
        request.reuseTab?.isConnected
      ? browser.addAdjacentTab(request.reuseTab, url, tabOptions)
      : browser.addTab(url, tabOptions);
    if (!current && request.workspaceId !== windowWorkspace(window)) {
      browser.hideTab(tab);
    }
    if (current && !inBackground) window.focus();
    return tab.linkedBrowser.browsingContext;
  },

  /** Called before the initial browser is constructed, including fresh windows. */
  prepareInitialWindow(
    rawWindow: unknown,
    options: InitialWindowOptions,
  ): number {
    const window = rawWindow as RoutingWindow;
    const workspace = WorkspaceSessions.isPrivateWindow(window)
      ? { id: null, userContextId: 0 }
      : defaultWorkspace();
    const requested = options.tabArgument
      ? actualContext(options.tabArgument)
      : options.openWindowInfo?.originAttributes?.userContextId ??
        options.userContextId ?? workspace.userContextId;
    if (
      typeof requested !== "number" || !Number.isSafeInteger(requested) ||
      requested < 0
    ) {
      throw new RangeError("Invalid initial window session");
    }
    const actual = WorkspaceSessions.isValidContext(requested) ? requested : 0;
    initialSessions.set(window, {
      workspaceId: options.tabArgument?.getAttribute(WORKSPACE_ATTRIBUTE) ||
        workspace.id,
      requestedUserContextId: requested,
      userContextId: actual,
    });
    // Initial URI loading must use the same actual browser context. Explicit 0
    // and origin contexts already won above; restored tabs keep their own state.
    if (window.arguments && window.arguments.length >= 3) {
      window.arguments[5] = actual;
    }
    return actual;
  },

  finishInitialTab(rawWindow: unknown, tab: RoutingTab): void {
    const state = initialSessions.get(rawWindow as RoutingWindow);
    if (state?.workspaceId && !tab.hasAttribute(WORKSPACE_ATTRIBUTE)) {
      tab.setAttribute(WORKSPACE_ATTRIBUTE, state.workspaceId);
    }
  },

  /** A missing initial identity must not make a first request in context 0. */
  prepareInitialLoad(rawWindow: unknown, uri: unknown): unknown {
    const window = rawWindow as RoutingWindow;
    const state = initialSessions.get(window);
    initialSessions.delete(window);
    if (
      !state || !uri || state.requestedUserContextId === state.userContextId
    ) return uri;
    const urls = Array.isArray(uri) ? uri : [uri];
    const tab = window.gBrowser?.selectedTab;
    if (!tab) return null;
    const firstURL = typeof urls[0] === "string" ? urls[0] : "about:blank";
    const pending = WorkspaceSessions.resolveNewTab(window, {
      userContextId: state.requestedUserContextId,
      floorpWorkspaceId: state.workspaceId,
      uriString: firstURL,
    });
    if (pending.pending) WorkspaceSessions.markPendingTab(tab, pending.pending);
    for (const url of urls.slice(1)) {
      if (typeof url === "string") {
        window.gBrowser?.addTab(url, {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
          userContextId: state.requestedUserContextId,
          floorpWorkspaceId: state.workspaceId,
          inBackground: true,
        });
      }
    }
    return "about:blank";
  },
};

export type WorkspaceExternalRoutingService = typeof WorkspaceExternalRouting;
