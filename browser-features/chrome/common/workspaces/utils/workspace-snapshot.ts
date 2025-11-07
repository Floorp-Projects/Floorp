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
        "resource:///modules/sessionstore/SessionStore.sys.mjs",
      ) as { SessionStore: SessionStoreModule };
      cache = module.SessionStore;
    }
    return cache;
  };
};

const sessionStoreImporter = createSessionStoreImporter();

export const ensureSessionStore = async (): Promise<SessionStoreModule> => {
  const sessionStore = sessionStoreImporter();
  try {
    if (sessionStore.promiseInitialized) {
      await sessionStore.promiseInitialized;
    }
  } catch (error) {
    console.error(
      "WorkspacesSnapshot: SessionStore initialization failed",
      error,
    );
  }
  return sessionStore;
};

const toNumber = (value: string | null, fallback = 0): number => {
  if (value === null) {
    return fallback;
  }
  const parsed = Number.parseInt(value, 10);
  return Number.isNaN(parsed) ? fallback : parsed;
};

const extractUrlFromState = (state: Record<string, unknown>): string | null => {
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
  if (typeof currentUrl === "string" && currentUrl.length > 0) {
    return currentUrl;
  }

  if (state) {
    return extractUrlFromState(state);
  }
  return null;
};

const toWorkspaceSnapshotTab = (
  tab: XULElement,
  state: Record<string, unknown> | null,
  dataManager: WorkspacesDataManagerBase,
): TWorkspaceSnapshotTab => {
  const pinned =
    tab.hasAttribute("pinned") ||
    Boolean((tab as unknown as { pinned?: boolean }).pinned);
  const sessionState = state ?? null;
  const title = extractTitleFromTab(tab, sessionState);
  const url = extractUrlFromTab(tab, sessionState);
  const userContextId = toNumber(tab.getAttribute("usercontextid"));
  const lastShownRaw = tab.getAttribute(WORKSPACE_LAST_SHOW_ID);
  const lastShownWorkspaceId =
    typeof lastShownRaw === "string" &&
    lastShownRaw.length > 0 &&
    dataManager.isWorkspaceID(lastShownRaw)
      ? lastShownRaw
      : null;

  return {
    state: sessionState,
    title,
    url,
    pinned,
    isSelected: tab === globalThis.gBrowser.selectedTab,
    userContextId,
    lastShownWorkspaceId,
  };
};

const parseTabState = (
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
    workspace: {
      workspaceId,
      name: workspace.name,
      icon: workspace.icon ?? null,
      userContextId: workspace.userContextId ?? 0,
    },
    tabs,
  };
};
