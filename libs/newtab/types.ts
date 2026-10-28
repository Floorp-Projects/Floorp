// SPDX-License-Identifier: MPL-2.0
export interface PinnedSite {
  url: string;
  title: string;
}
export interface BackgroundSettings {
  type: "none" | "random" | "custom" | "folderPath" | "floorp";
  customImage: string | null;
  fileName: string | null;
  folderPath: string | null;
  selectedFloorp: string | null;
  slideshowEnabled: boolean;
  slideshowInterval: number;
}
export interface NewTabSettings {
  schemaVersion: number;
  revision: number;
  components: {
    topSites: boolean;
    clock: boolean;
    searchBar: boolean;
    firefoxLayout: boolean;
  };
  background: BackgroundSettings;
  searchBar: { searchEngine: string };
  topSites: { pinned: PinnedSite[]; blocked: string[] };
  speedDial: {
    rootGuid: string | null;
    commonGuid: string | null;
    workspaceGroups: Record<string, string>;
    show: boolean;
    columns: 4 | 5 | 6;
    tileSize: "standard" | "compact";
  };
  home: {
    showHistory: boolean;
    showFeed: boolean;
    layout: "balanced" | "links" | "reading";
    translucentPanels: boolean;
  };
  rss: {
    refreshMinutes: 0 | 15 | 30 | 60;
    retentionDays: number;
    maxItems: number;
  };
  sponsors: { enabled: boolean; dismissedIds: string[] };
}
export type NewTabSettingsPatch = {
  [K in Exclude<keyof NewTabSettings, "schemaVersion" | "revision">]?: Partial<
    NewTabSettings[K]
  >;
};
export interface BookmarkNode {
  guid: string;
  parentGuid: string;
  title: string;
  type: "bookmark" | "folder";
  url?: string;
  index: number;
  childCount?: number;
}
export interface FolderView {
  guid: string;
  title: string;
  groupGuid: string;
  ancestors: BookmarkNode[];
  children: BookmarkNode[];
  revision: number;
}
export interface DeleteSummary {
  guid: string;
  folders: number;
  bookmarks: number;
  revision: number;
}
export type BookmarkCommand =
  | {
    action: "create";
    type: "bookmark" | "folder";
    parentGuid: string;
    title: string;
    url?: string;
  }
  | { action: "update"; guid: string; title: string; url?: string }
  | { action: "move"; guid: string; parentGuid: string; index?: number }
  | { action: "reorder"; parentGuid: string; guids: string[] }
  | { action: "delete"; guid: string; expectedRevision: number };
export interface WorkspaceInfo {
  id: string;
  title: string;
}
export interface Sponsor {
  id: string;
  title: string;
  url: string;
  icon?: string;
}
export interface FeedSummary {
  id: string;
  url: string;
  title: string;
  siteUrl: string | null;
  enabled: boolean;
  unreadCount: number;
  updating?: boolean;
  lastSuccessAt: number | null;
  error: string | null;
}
export type SafeTag =
  | "text"
  | "p"
  | "h1"
  | "h2"
  | "h3"
  | "h4"
  | "ul"
  | "ol"
  | "li"
  | "strong"
  | "em"
  | "blockquote"
  | "pre"
  | "code"
  | "br"
  | "hr"
  | "a"
  | "img";
export interface SafeNode {
  tag: SafeTag;
  text?: string;
  href?: string;
  imageId?: string;
  alt?: string;
  children?: SafeNode[];
}
export interface ArticleSummary {
  feedTitle?: string;
  feedEnabled?: boolean;
  siteUrl?: string | null;
  id: string;
  feedId: string;
  title: string;
  link: string | null;
  author: string;
  publishedAt: number | null;
  receivedAt: number;
  excerpt: string;
  isRead: boolean;
  starred: boolean;
  hasFullContent: boolean;
}
export interface Article extends ArticleSummary {
  content: SafeNode[];
}
export interface ArticleQuery {
  feedId?: string;
  filter?: "all" | "unread" | "starred";
  query?: string;
  offset?: number;
  limit?: number;
}
export interface ArticlePage {
  items: ArticleSummary[];
  total: number;
  nextOffset: number | null;
}
export type SubscribeResult = { type: "subscribed"; feed: FeedSummary } | {
  type: "discovery";
  candidates: { title: string; url: string }[];
};
export interface NewTabSnapshot {
  settings: NewTabSettings;
  groups: BookmarkNode[];
  workspace: WorkspaceInfo | null;
  workspaces: WorkspaceInfo[];
  sponsors: Sponsor[];
  feeds: FeedSummary[];
  readOnly: boolean;
  private: boolean;
  error?: string;
}
export interface NewTabOperations {
  repairBookmarks: { input: Record<string, never>; output: null };
  snapshot: { input: Record<string, never>; output: NewTabSnapshot };
  patchSettings: {
    input: { expectedRevision: number; changes: NewTabSettingsPatch };
    output: NewTabSettings;
  };
  folder: { input: { guid: string }; output: FolderView };
  deleteSummary: { input: { guid: string }; output: DeleteSummary };
  bookmark: { input: BookmarkCommand; output: { guid?: string } };
  feeds: { input: Record<string, never>; output: FeedSummary[] };
  articles: { input: ArticleQuery; output: ArticlePage };
  article: { input: { id: string }; output: Article };
  subscribe: { input: { url: string }; output: SubscribeResult };
  unsubscribe: { input: { id: string }; output: null };
  refresh: { input: { ids?: string[] }; output: null };
  articleState: {
    input: { ids: string[]; isRead?: boolean; starred?: boolean };
    output: null;
  };
  articleImage: {
    input: { articleId: string; imageId: string };
    output: string;
  };
  dismissSponsor: { input: { id: string }; output: NewTabSettings };
  restoreSponsors: { input: Record<string, never>; output: NewTabSettings };
  saveSponsor: {
    input: {
      id: string;
      parentGuid: string;
      expectedTitle: string;
      expectedUrl: string;
    };
    output: { guid?: string };
  };
  focusUrlBar: { input: Record<string, never>; output: null };
  openHub: { input: Record<string, never>; output: null };
  history: { input: Record<string, never>; output: PinnedSite[] };
  pickFolder: { input: Record<string, never>; output: { path: string | null } };
  folderImage: {
    input: Record<string, never>;
    output: { image: string | null; fileName: string | null };
  };
}
export type NewTabMethod = keyof NewTabOperations;
export type NewTabRequest = {
  [K in NewTabMethod]: { method: K; params: NewTabOperations[K]["input"] };
}[NewTabMethod];
export type NewTabResponse<T = unknown> = { ok: true; value: T } | {
  ok: false;
  error: { code: string; message: string };
};
export interface NewTabChangedEvent {
  topic: "settings" | "bookmarks" | "feeds" | "workspace";
  revision: number;
}
