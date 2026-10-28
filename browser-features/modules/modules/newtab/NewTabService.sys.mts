// SPDX-License-Identifier: MPL-2.0
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
) as {
  setTimeout: (callback: () => void, milliseconds: number) => number;
  clearTimeout: (timer: number) => void;
};
import type {
  NewTabChangedEvent,
  NewTabRequest,
  NewTabSnapshot,
} from "#libs/newtab/types.ts";
import { validWorkspaceAssignments } from "#libs/newtab/assignments.ts";
import { SettingsService } from "./SettingsService.sys.mts";
import { BookmarkService } from "./BookmarkService.sys.mts";
import { SponsorService } from "./SponsorService.sys.mts";
import { FeedService } from "../feeds/FeedService.sys.mts";
import {
  focusUrlBar,
  getFolderImage,
  getHistorySites,
  getWorkspaceSnapshot,
  openHub,
  pickBackgroundFolder,
} from "./BrowserIntegration.sys.mts";
import type { BrowserWindowContext } from "./BrowserIntegration.sys.mts";

interface RequestContext {
  window: BrowserWindowContext;
  browsingContext: BrowsingContext;
}
let revision = 0;
const listeners = new Set<(event: NewTabChangedEvent) => void>();
let stops: (() => void)[] = [];
const pendingTopics = new Set<NewTabChangedEvent["topic"]>();
let notifyTimer: ReturnType<typeof setTimeout> | null = null;
let bookmarksLoading = 0;
let bookmarksChangedDuringLoad = false;
const notify = (topic: NewTabChangedEvent["topic"]) => {
  if (topic === "bookmarks" && bookmarksLoading) {
    bookmarksChangedDuringLoad = true;
    return;
  }
  pendingTopics.add(topic);
  if (notifyTimer) return;
  notifyTimer = setTimeout(() => {
    notifyTimer = null;
    const topics = [...pendingTopics];
    pendingTopics.clear();
    for (const next of topics) {
      const event = { topic: next, revision: ++revision };
      for (const listener of listeners) {
        try {
          listener(event);
        } catch { /* Closed actors cannot block other pages. */ }
      }
    }
  }, 20);
};
async function ensureBookmarks(): Promise<void> {
  bookmarksLoading++;
  try {
    await BookmarkService.ensure();
  } finally {
    bookmarksLoading--;
    if (!bookmarksLoading && bookmarksChangedDuringLoad) {
      bookmarksChangedDuringLoad = false;
      notify("bookmarks");
    }
  }
}
function startObservers(): void {
  if (stops.length) return;
  stops.push(SettingsService.subscribe(() => notify("settings")));
  stops.push(BookmarkService.subscribe(() => notify("bookmarks")));
  stops.push(FeedService.subscribeChanges(() => notify("feeds")));
  const workspace = {
    observe() {
      notify("workspace");
    },
  };
  Services.obs.addObserver(workspace, "floorp.workspaces.changed");
  const changed = {
    observe() {
      notify("settings");
    },
  };
  for (
    const key of [
      "floorp.design.configs",
      "floorp.workspaces.v4.store",
      "floorp.workspaces.enabled",
      "floorp.newtab.sponsorCatalog",
    ]
  ) Services.prefs.addObserver(key, changed);
  stops.push(() => {
    Services.obs.removeObserver(workspace, "floorp.workspaces.changed");
    for (
      const key of [
        "floorp.design.configs",
        "floorp.workspaces.v4.store",
        "floorp.workspaces.enabled",
        "floorp.newtab.sponsorCatalog",
      ]
    ) Services.prefs.removeObserver(key, changed);
  });
}
export const NewTabService = {
  subscribe(listener: (event: NewTabChangedEvent) => void): () => void {
    startObservers();
    listeners.add(listener);
    return () => {
      listeners.delete(listener);
      if (!listeners.size) {
        const cleanup = stops;
        stops = [];
        if (notifyTimer) clearTimeout(notifyTimer);
        notifyTimer = null;
        pendingTopics.clear();
        cleanup.forEach((stop) => stop());
      }
    };
  },
  async snapshot(window: BrowserWindowContext): Promise<NewTabSnapshot> {
    let error: string | undefined;
    try {
      await ensureBookmarks();
    } catch (cause) {
      error = cause instanceof Error
        ? cause.message
        : "Speed Dial could not be loaded.";
    }
    let groups: NewTabSnapshot["groups"] = [],
      feeds: NewTabSnapshot["feeds"] = [];
    try {
      groups = await BookmarkService.groups();
    } catch {
      error ??= "Speed Dial could not be loaded.";
    }
    try {
      feeds = await FeedService.listFeeds();
    } catch {
      error ??=
        "The feed store could not be opened. Existing data has been kept.";
    }
    const settings = SettingsService.get();
    return {
      settings,
      groups,
      ...getWorkspaceSnapshot(window),
      sponsors: SponsorService.list(),
      feeds,
      readOnly: false,
      private: false,
      ...(error ? { error } : {}),
    };
  },
  async invoke(
    request: NewTabRequest,
    context: RequestContext,
  ): Promise<unknown> {
    switch (request.method) {
      case "snapshot":
        return this.snapshot(context.window);
      case "repairBookmarks":
        await BookmarkService.ensure({ repair: true });
        return null;
      case "patchSettings": {
        const { changes, expectedRevision } = request.params;
        if (changes?.speedDial?.workspaceGroups) {
          const groups = await BookmarkService.groups();
          const workspaces = getWorkspaceSnapshot(context.window).workspaces;
          changes.speedDial.workspaceGroups = validWorkspaceAssignments(
            changes.speedDial.workspaceGroups,
            SettingsService.get().speedDial.workspaceGroups,
            workspaces.map((item) => item.id),
            groups.map((item) => item.guid),
          );
        }
        return SettingsService.patch(expectedRevision, changes);
      }
      case "folder":
        return BookmarkService.folder(request.params.guid);
      case "deleteSummary":
        return BookmarkService.deleteSummary(request.params.guid);
      case "bookmark":
        return BookmarkService.mutate(request.params);
      case "feeds":
        return FeedService.listFeeds();
      case "articles":
        return FeedService.queryArticles(request.params);
      case "article":
        return FeedService.getArticle(request.params.id);
      case "subscribe":
        return FeedService.subscribe(request.params.url);
      case "unsubscribe":
        await FeedService.unsubscribe(request.params.id);
        return null;
      case "refresh":
        if (Services.io.offline) {
          throw Object.assign(
            new Error(
              "The browser is offline. Saved articles remain available.",
            ),
            { code: "NETWORK" },
          );
        }
        await FeedService.start();
        void FeedService.refresh(request.params.ids).catch(() =>
          notify("feeds")
        );
        return null;
      case "articleState":
        await FeedService.patchArticleState(request.params.ids, request.params);
        return null;
      case "articleImage":
        return FeedService.loadArticleImage(
          request.params.articleId,
          request.params.imageId,
        );
      case "dismissSponsor":
        return SponsorService.dismiss(request.params.id);
      case "restoreSponsors":
        return SponsorService.restore();
      case "saveSponsor":
        return SponsorService.save(
          request.params.id,
          request.params.parentGuid,
          request.params.expectedTitle,
          request.params.expectedUrl,
        );
      case "focusUrlBar":
        focusUrlBar(context.window);
        return null;
      case "openHub":
        openHub(context.window);
        return null;
      case "history":
        return getHistorySites();
      case "pickFolder":
        return pickBackgroundFolder(context.browsingContext);
      case "folderImage":
        return getFolderImage();
    }
  },
};
