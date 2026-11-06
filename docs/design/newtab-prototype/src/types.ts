export type Route = "home" | "dial" | "feed" | "hub";
export type GroupId = "common" | "development" | "design" | "reading";
export type WorkspaceId = "development" | "design" | "personal";
export type HubTab = "general" | "dial" | "feed" | "sponsors";
export type FeedFilter = "all" | "unread" | "starred";

export type BackgroundType =
  | "none"
  | "random"
  | "custom"
  | "folderPath"
  | "floorp";

export interface BackgroundSettings {
  type: BackgroundType;
  customImage: string | null;
  fileName: string | null;
  folderPath: string | null;
  selectedFloorp: string | null;
  slideshowEnabled: boolean;
  slideshowInterval: number;
}

export interface WallpaperAsset {
  name: string;
  label: string;
  url: string;
}

export interface Site {
  id: string;
  title: string;
  url: string;
  icon: string;
  groupId: GroupId;
  folderId?: string | null;
  order?: number;
  sponsorId?: string;
}

export interface DialFolder {
  id: string;
  title: string;
  groupId: GroupId;
  parentId: string | null;
  order: number;
}

export type DialEntry =
  | { type: "site"; id: string; site: Site; order: number }
  | { type: "folder"; id: string; folder: DialFolder; order: number };

export interface FolderDestination {
  value: string;
  label: string;
  groupId: GroupId;
  folderId: string | null;
}

export interface Sponsor {
  id: string;
  title: string;
  url: string;
  icon: string;
}

export interface Article {
  id: string;
  feedId: string;
  url: string;
  title: string;
  excerpt: string;
  body: string[];
  date: string;
  relativeTime: string;
  author: string;
  isExcerpt: boolean;
  sectionHeadings?: string[];
}

export interface Feed {
  id: string;
  title: string;
  url: string;
  icon: string;
}

export interface Workspace {
  id: WorkspaceId;
  title: string;
}

export interface Group {
  id: GroupId;
  title: string;
}

export interface Settings {
  showSearch: boolean;
  showDials: boolean;
  showFeed: boolean;
  showClock: boolean;
  layout: "balanced" | "links" | "reading";
  background: BackgroundSettings;
  translucentPanels: boolean;
  columns: 4 | 5 | 6;
  tileSize: "standard" | "compact";
  showSponsors: boolean;
  refreshMinutes: number;
}

export interface PrototypeState {
  settings: Settings;
  assignments: Record<WorkspaceId, GroupId>;
  sites: Site[];
  folders: DialFolder[];
  feeds: Feed[];
  readIds: string[];
  starredIds: string[];
  dismissedSponsors: string[];
}
