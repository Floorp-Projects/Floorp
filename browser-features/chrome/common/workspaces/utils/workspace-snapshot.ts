/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type {
  TWorkspaceID,
  TWorkspaceSnapshot,
  TWorkspaceSnapshotTab,
} from "./type.ts";
import {
  WORKSPACE_LAST_SHOW_ID,
  WORKSPACE_TAB_ATTRIBUTION_ID,
} from "./workspaces-static-names.ts";
import type { WorkspacesDataManagerBase } from "../workspacesDataManagerBase.tsx";
import type { RestorableTab } from "./backup-types.ts";
import {
  isRestorableTabUrl,
  sanitizeTabState,
} from "./workspaces-backup-helpers.ts";

interface SessionStoreModule {
  promiseInitialized: Promise<void> | undefined;
  getTabState(tab: XULElement): string;
  setTabState(tab: XULElement, state: string): void;
}

type SessionStoreImporter = () => SessionStoreModule;

const createSessionStoreImporter = (): SessionStoreImporter => {
  let cache: SessionStoreModule | null = null;
  return () => {
    if (!cache) {
      const module = ChromeUtils.importESModule(
        "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
      ) as { SessionStore: SessionStoreModule };
      cache = module.SessionStore;
    }
    return cache;
  };
};

const sessionStoreImporter = createSessionStoreImporter();

/**
 * Resolves SessionStore and waits for its initialization.
 *
 * Rejections are propagated on purpose: capturing or restoring tabs from a
 * half-initialized SessionStore silently produces empty tab states, so callers
 * must fail closed instead.
 */
export const ensureSessionStore = async (): Promise<SessionStoreModule> => {
  const sessionStore = sessionStoreImporter();
  if (sessionStore.promiseInitialized) {
    await sessionStore.promiseInitialized;
  }
  return sessionStore;
};

export const toNumber = (value: string | null, fallback = 0): number => {
  if (value === null) {
    return fallback;
  }
  const parsed = Number.parseInt(value, 10);
  return Number.isNaN(parsed) ? fallback : parsed;
};

export const extractUrlFromState = (
  state: Record<string, unknown>,
): string | null => {
  const entries = state.entries;
  if (!Array.isArray(entries)) {
    return null;
  }

  const indexRaw = state.index;
  if (
    typeof indexRaw === "number" &&
    indexRaw > 0 &&
    indexRaw <= entries.length
  ) {
    const focused = entries[indexRaw - 1];
    if (focused && typeof focused === "object" && "url" in focused) {
      const urlValue = (focused as { url?: unknown }).url;
      if (typeof urlValue === "string") {
        return urlValue;
      }
    }
  }

  for (let i = entries.length - 1; i >= 0; i--) {
    const entry = entries[i];
    if (entry && typeof entry === "object" && "url" in entry) {
      const urlValue = (entry as { url?: unknown }).url;
      if (typeof urlValue === "string") {
        return urlValue;
      }
    }
  }
  return null;
};

export const createWorkspaceSnapshotMetadata = (
  workspaceId: TWorkspaceID,
  workspace: {
    name: string;
    icon?: string | null | undefined;
    userContextId: number;
  },
): TWorkspaceSnapshot["workspace"] => {
  const metadata: TWorkspaceSnapshot["workspace"] = {
    workspaceId,
    name: workspace.name,
    userContextId: workspace.userContextId ?? 0,
  };
  if (Object.hasOwn(workspace, "icon")) {
    metadata.icon = workspace.icon;
  }
  return metadata;
};

const extractTitleFromTab = (
  tab: XULElement,
  state: Record<string, unknown> | null,
): string | null => {
  const label = tab.getAttribute("label");
  if (label) {
    return label;
  }

  const browser = (
    tab as unknown as {
      linkedBrowser?: {
        contentTitle?: string | null;
      };
    }
  ).linkedBrowser;
  if (browser?.contentTitle) {
    return browser.contentTitle;
  }

  if (state && Array.isArray(state.entries)) {
    for (let i = state.entries.length - 1; i >= 0; i--) {
      const entry = state.entries[i];
      if (entry && typeof entry === "object" && "title" in entry) {
        const titleValue = (entry as { title?: unknown }).title;
        if (typeof titleValue === "string") {
          return titleValue;
        }
      }
    }
  }
  return null;
};

const extractUrlFromTab = (
  tab: XULElement,
  state: Record<string, unknown> | null,
): string | null => {
  const browser = (
    tab as unknown as {
      linkedBrowser?: {
        currentURI?: { spec?: string | null };
      };
    }
  ).linkedBrowser;
  const currentUrl = browser?.currentURI?.spec;
  // A pending (not yet restored) tab can report about:blank as its current
  // URI while its session state holds the real page; prefer the state then.
  const isPlaceholder = tab.hasAttribute("pending") ||
    currentUrl === "about:blank";
  if (isPlaceholder && state) {
    const stateUrl = extractUrlFromState(state);
    if (stateUrl) {
      return stateUrl;
    }
  }
  if (typeof currentUrl === "string" && currentUrl.length > 0) {
    return currentUrl;
  }

  if (state) {
    return extractUrlFromState(state);
  }
  return null;
};

export const toWorkspaceSnapshotTab = (
  tab: XULElement,
  state: Record<string, unknown> | null,
  dataManager: Pick<WorkspacesDataManagerBase, "isWorkspaceID">,
  isSelected = tab === globalThis.gBrowser.selectedTab,
): TWorkspaceSnapshotTab => {
  const pinned = tab.hasAttribute("pinned") ||
    Boolean((tab as unknown as { pinned?: boolean }).pinned);
  const sessionState = state ?? null;
  const title = extractTitleFromTab(tab, sessionState);
  const url = extractUrlFromTab(tab, sessionState);
  const userContextId = toNumber(tab.getAttribute("usercontextid"));
  const lastShownRaw = tab.getAttribute(WORKSPACE_LAST_SHOW_ID);
  const lastShownWorkspaceId = typeof lastShownRaw === "string" &&
      lastShownRaw.length > 0 &&
      dataManager.isWorkspaceID(lastShownRaw)
    ? lastShownRaw
    : null;

  return {
    state: sessionState,
    title,
    url,
    pinned,
    isSelected,
    userContextId,
    lastShownWorkspaceId,
  };
};

export const parseTabState = (
  sessionStore: SessionStoreModule,
  tab: XULElement,
): Record<string, unknown> | null => {
  try {
    const stateString = sessionStore.getTabState(tab);
    const parsed = JSON.parse(stateString) as unknown;
    if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) {
      return parsed as Record<string, unknown>;
    }
  } catch (error) {
    console.error("WorkspacesSnapshot: Failed to parse tab state", error);
  }
  return null;
};

export const createWorkspaceSnapshot = async (
  workspaceId: TWorkspaceID,
  dataManager: WorkspacesDataManagerBase,
): Promise<TWorkspaceSnapshot | null> => {
  if (!dataManager.isWorkspaceID(workspaceId)) {
    return null;
  }

  const workspace = dataManager.getRawWorkspace(workspaceId);
  if (!workspace) {
    return null;
  }

  const sessionStore = await ensureSessionStore();

  const tabNodes = document?.querySelectorAll(
    `[${WORKSPACE_TAB_ATTRIBUTION_ID}="${workspaceId}"]`,
  ) as NodeListOf<XULElement> | undefined;

  const tabs: TWorkspaceSnapshotTab[] = [];
  if (tabNodes) {
    for (const tab of tabNodes) {
      const state = parseTabState(sessionStore, tab);
      tabs.push(toWorkspaceSnapshotTab(tab, state, dataManager));
    }
  }

  return {
    capturedAt: Date.now(),
    workspace: createWorkspaceSnapshotMetadata(workspaceId, workspace),
    tabs,
  };
};

/* -------------------------------------------------------------------------
 * Restore primitive
 * ---------------------------------------------------------------------- */

/** Subset of `gBrowser` needed to recreate tabs in a target window. */
export interface RestoreTargetBrowser {
  addTab(
    url: string,
    options: {
      skipAnimation?: boolean;
      inBackground?: boolean;
      userContextId?: number;
      triggeringPrincipal?: unknown;
    },
  ): XULElement;
  getBrowserForTab(tab: XULElement): {
    loadURI(uri: unknown, options?: unknown): void;
  };
  pinTab(tab: XULElement): void;
  removeTab?(tab: XULElement): void;
  selectedTab: XULElement | null;
  tabs: XULElement[];
}

export interface RestoreTabOutcome {
  /** Index of the tab inside the restored array. */
  index: number;
  ok: boolean;
  error?: string;
  /** True when the tab was intentionally not opened (not a failure). */
  skipped?: true;
  reason?: "unsupported-url";
  /** The created tab element, when creation succeeded. */
  tab?: XULElement;
}

export interface RestoreTabsOptions<T extends RestorableTab> {
  /** Window that receives the tabs. */
  gBrowser: RestoreTargetBrowser;
  /**
   * Workspace assigned to each restored tab. Return `null` to leave the tab
   * unattributed (tabs mode / workspaces disabled).
   */
  resolveWorkspaceId: (tab: T) => TWorkspaceID | null;
  /**
   * Skip the URL allowlist and state sanitization and load with the system
   * principal, as the workspace archive restore did before backups existed.
   * Backup files are untrusted input and must leave this unset.
   */
  trustedSource?: boolean;
}

/**
 * Fixed internal pages the allowlist accepts. They are not web content, so a
 * null principal cannot load them; the system principal is safe here because
 * the target is one of these exact pages, never a file-derived arbitrary URL.
 */
const INTERNAL_RESTORE_URLS = new Set([
  "about:blank",
  "about:newtab",
  "about:home",
]);

/** `about:NewTab?x#y` → `about:newtab`; null for other schemes or bad input. */
const toAboutPage = (url: string): string | null => {
  try {
    const parsed = new URL(url);
    return parsed.protocol === "about:"
      ? `about:${parsed.pathname.toLowerCase()}`
      : null;
  } catch {
    return null;
  }
};

const isExtensionUrl = (url: string): boolean => {
  try {
    return new URL(url).protocol === "moz-extension:";
  } catch {
    return false;
  }
};

/**
 * Triggering principal for loading `url` from a backup file:
 * - the fixed internal pages above: system principal (null cannot load them);
 * - moz-extension pages: the extension's own content principal, so the page
 *   loads as if it opened itself and never with system privileges;
 * - web URLs: a null principal scoped to the tab's container, like a link from
 *   an opaque origin.
 * With `trustedSource` the archive behavior (system principal) is kept.
 */
const createRestorePrincipal = (
  url: string,
  userContextId: number,
  trustedSource: boolean,
): unknown => {
  const ssm = Services.scriptSecurityManager;
  const aboutPage = toAboutPage(url);
  if (trustedSource || (aboutPage && INTERNAL_RESTORE_URLS.has(aboutPage))) {
    return ssm.getSystemPrincipal();
  }
  if (isExtensionUrl(url)) {
    try {
      return ssm.createContentPrincipal(Services.io.newURI(url), {
        userContextId,
      });
    } catch {
      // Fall through to the null principal.
    }
  }
  return ssm.createNullPrincipal({ userContextId });
};

/**
 * SessionStore restores each history entry with its stored
 * `triggeringPrincipal_base64`, falling back to a null principal, which cannot
 * load internal or extension pages. Backup files never supply principals (the
 * sanitizer drops them), so derive one per top-level entry from its URL with
 * the same rules as the tab load. Web entries keep the null-principal
 * fallback. Child (subframe) entries are left alone: an extension principal
 * there would let a crafted file embed a non-web-accessible extension page.
 */
const attachEntryPrincipals = (
  entries: Array<Record<string, unknown>>,
  userContextId: number,
): void => {
  const e10s = (
    globalThis as unknown as {
      E10SUtils?: { serializePrincipal?: (principal: unknown) => string };
    }
  ).E10SUtils;
  if (!e10s || typeof e10s.serializePrincipal !== "function") {
    return;
  }
  for (const entry of entries) {
    const url = typeof entry.url === "string" ? entry.url : "";
    const aboutPage = toAboutPage(url);
    if (
      (aboutPage && INTERNAL_RESTORE_URLS.has(aboutPage)) ||
      isExtensionUrl(url)
    ) {
      try {
        entry.triggeringPrincipal_base64 = e10s.serializePrincipal(
          createRestorePrincipal(url, userContextId, false),
        );
      } catch (error) {
        console.error(
          "WorkspacesSnapshot: failed to derive entry principal",
          error,
        );
      }
    }
  }
};

/**
 * Sanitizes a stored tab state for restore and removes history entries whose
 * URL is not restorable. Returns null when no usable entry remains, so the
 * caller falls back to loading the tab URL.
 */
const sanitizeRestoreState = (
  state: Record<string, unknown>,
  userContextId: number,
): Record<string, unknown> | null => {
  const sanitized = sanitizeTabState(structuredClone(state));
  if (!sanitized) {
    return null;
  }
  const entries = Array.isArray(sanitized.entries)
    ? (sanitized.entries as Array<Record<string, unknown> | null>)
    : [];
  const kept = entries.filter((entry) =>
    !!entry && typeof entry.url === "string" && isRestorableTabUrl(entry.url)
  );
  if (kept.length === 0) {
    return null;
  }
  if (kept.length !== entries.length) {
    sanitized.entries = kept;
    const index = typeof sanitized.index === "number"
      ? sanitized.index
      : kept.length;
    sanitized.index = Math.min(Math.max(1, index), kept.length);
  }
  attachEntryPrincipals(
    sanitized.entries as Array<Record<string, unknown>>,
    userContextId,
  );
  return sanitized;
};

/**
 * Opens tabs in `options.gBrowser` without touching the workspace store.
 *
 * Every tab is reported individually so destructive callers can decide whether
 * to proceed (for example, only close replaced tabs when every restore
 * succeeded). Failures never throw out of this function.
 */
export const restoreTabsIntoWindow = async <T extends RestorableTab>(
  tabs: T[],
  options: RestoreTabsOptions<T>,
): Promise<RestoreTabOutcome[]> => {
  if (tabs.length === 0) {
    return [];
  }

  const sessionStore = await ensureSessionStore();
  const gBrowser = options.gBrowser;
  const outcomes: RestoreTabOutcome[] = [];
  let targetTab: XULElement | null = null;

  for (let index = 0; index < tabs.length; index++) {
    const tabSnapshot = tabs[index];
    let ok = true;
    let errorMessage: string | undefined;

    const recordFailure = (error: unknown, label: string): void => {
      ok = false;
      errorMessage = error instanceof Error ? error.message : String(error);
      console.error(`WorkspacesSnapshot: failed to restore ${label}`, error);
    };

    const trusted = options.trustedSource === true;

    // Backup files are untrusted input: never open a URL the allowlist rejects.
    if (!trusted && tabSnapshot.url && !isRestorableTabUrl(tabSnapshot.url)) {
      outcomes.push({
        index,
        ok: false,
        skipped: true,
        reason: "unsupported-url",
      });
      continue;
    }

    // Re-sanitize the session state and drop history entries with unsafe URLs.
    let safeState: Record<string, unknown> | null = null;
    if (tabSnapshot.state) {
      try {
        safeState = trusted
          ? structuredClone(tabSnapshot.state)
          : sanitizeRestoreState(
            tabSnapshot.state,
            tabSnapshot.userContextId,
          );
      } catch (error) {
        console.error(
          "WorkspacesSnapshot: failed to sanitize tab state",
          error,
        );
        safeState = null;
      }
    }
    const loadUrl = tabSnapshot.url ?? "about:blank";

    // Never load a file-derived web URL with the system principal; see
    // createRestorePrincipal for the per-scheme choice.
    const triggeringPrincipal = createRestorePrincipal(
      loadUrl,
      tabSnapshot.userContextId,
      trusted,
    );

    let tab: XULElement;
    try {
      // With a session state the tab starts as a blank placeholder that
      // SessionStore.setTabState then replaces (avoids loading the URL twice).
      tab = gBrowser.addTab(safeState ? "about:blank" : loadUrl, {
        skipAnimation: true,
        inBackground: true,
        userContextId: tabSnapshot.userContextId,
        triggeringPrincipal: safeState
          ? Services.scriptSecurityManager.getSystemPrincipal()
          : triggeringPrincipal,
      });
    } catch (error) {
      recordFailure(error, "tab");
      outcomes.push({ index, ok: false, error: errorMessage });
      continue;
    }

    const workspaceId = options.resolveWorkspaceId(tabSnapshot);

    try {
      if (workspaceId) {
        tab.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, workspaceId);
      } else {
        tab.removeAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
      }
      tab.setAttribute("usercontextid", String(tabSnapshot.userContextId));

      if (workspaceId && tabSnapshot.lastShownWorkspaceId) {
        tab.setAttribute(
          WORKSPACE_LAST_SHOW_ID,
          tabSnapshot.lastShownWorkspaceId,
        );
      } else {
        tab.removeAttribute(WORKSPACE_LAST_SHOW_ID);
      }
    } catch (error) {
      recordFailure(error, "tab attributes");
    }

    if (safeState) {
      try {
        const clonedState = safeState;
        const attributes =
          (clonedState.attributes as Record<string, unknown> | undefined) ??
            {};
        if (workspaceId) {
          attributes[WORKSPACE_TAB_ATTRIBUTION_ID] = workspaceId;
        } else if (WORKSPACE_TAB_ATTRIBUTION_ID in attributes) {
          delete attributes[WORKSPACE_TAB_ATTRIBUTION_ID];
        }
        if (workspaceId && tabSnapshot.lastShownWorkspaceId) {
          attributes[WORKSPACE_LAST_SHOW_ID] = tabSnapshot.lastShownWorkspaceId;
        } else if (WORKSPACE_LAST_SHOW_ID in attributes) {
          delete attributes[WORKSPACE_LAST_SHOW_ID];
        }
        // Keep the top-level representation consistent with `attributes`.
        if (workspaceId) {
          clonedState[WORKSPACE_TAB_ATTRIBUTION_ID] = workspaceId;
        } else if (WORKSPACE_TAB_ATTRIBUTION_ID in clonedState) {
          delete clonedState[WORKSPACE_TAB_ATTRIBUTION_ID];
        }
        if (workspaceId && tabSnapshot.lastShownWorkspaceId) {
          clonedState[WORKSPACE_LAST_SHOW_ID] =
            tabSnapshot.lastShownWorkspaceId;
        } else if (WORKSPACE_LAST_SHOW_ID in clonedState) {
          delete clonedState[WORKSPACE_LAST_SHOW_ID];
        }
        clonedState.attributes = attributes;
        sessionStore.setTabState(tab, JSON.stringify(clonedState));
      } catch (error) {
        recordFailure(error, "tab state");
      }
    } else if (tabSnapshot.url) {
      try {
        gBrowser.getBrowserForTab(tab).loadURI(
          Services.io.newURI(tabSnapshot.url),
          { triggeringPrincipal },
        );
      } catch (error) {
        recordFailure(error, "tab URL");
      }
    }

    if (tabSnapshot.pinned) {
      try {
        gBrowser.pinTab(tab);
      } catch (error) {
        recordFailure(error, "tab pin");
      }
    }

    if (tabSnapshot.isSelected) {
      targetTab = tab;
    }

    outcomes.push({ index, ok, error: errorMessage, tab });
  }

  if (targetTab) {
    try {
      gBrowser.selectedTab = targetTab;
    } catch (error) {
      console.error("WorkspacesSnapshot: failed to select restored tab", error);
    }
  }

  return outcomes;
};
