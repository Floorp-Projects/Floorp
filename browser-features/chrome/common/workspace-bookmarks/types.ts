// SPDX-License-Identifier: MPL-2.0

export interface BookmarkVisibilityContext {
  workspaceId: string | null;
  enabled: boolean;
  scopeEnabled: boolean;
  ready: boolean;
  generation: number;
  windowGeneration: number;
  windowId: number | null;
  allItems: boolean;
}

export interface ScopedBookmark {
  guid: string;
  parentGuid: string;
  title: string | null;
  url: string;
  type: number;
}

export interface EffectiveBookmarkScope {
  workspaceIds: readonly string[] | null;
  inheritedFrom: readonly string[];
}

export interface ScopedBookmarkFolder {
  guid: string;
  title: string;
  path: string;
  scope: readonly string[] | null;
  effectiveScope: EffectiveBookmarkScope;
}

export interface BookmarkImportPreview {
  generation: number;
  knownWorkspaceIds: readonly string[];
  settings: object;
  applicableRules: Array<{ guid: string; workspaceIds: readonly string[] }>;
  unresolvedFolders: string[];
  unresolvedWorkspaces: string[];
  changes: Array<{
    guid: string;
    title: string;
    before: readonly string[] | null;
    after: readonly string[] | null;
  }>;
  sourceMatches: boolean;
}

export interface BookmarkImportMapping {
  folders?: Record<string, string>;
  workspaces?: Record<string, string>;
}

export interface WorkspaceBookmarksService {
  readonly ready: Promise<void>;
  setWindowContext(
    win: Window,
    workspaceId: string | null,
    enabled: boolean,
  ): void;
  clearWindowContext(win: Window): void;
  getContext(
    win: Window,
    options?: { allItems?: boolean },
  ): BookmarkVisibilityContext;
  isVisible(guid: string, context: Window | BookmarkVisibilityContext): boolean;
  isUrlVisible(
    url: string,
    context: Window | BookmarkVisibilityContext,
  ): boolean;
  visibleBookmarksForUrl(
    url: string,
    context: Window | BookmarkVisibilityContext,
  ): Promise<ScopedBookmark[]>;
  getVisibleBookmarkGuids(
    context: Window | BookmarkVisibilityContext,
  ): string[] | null;
  getScope(guid: string): readonly string[] | null;
  getEffectiveScope(guid: string): EffectiveBookmarkScope;
  setFolderScope(
    guid: string,
    workspaceIds: readonly string[] | null,
  ): Promise<void>;
  listFolders(): Promise<ScopedBookmarkFolder[]>;
  getEnabled(): boolean;
  setEnabled(enabled: boolean): void;
  getStatus(): {
    state: "loading" | "ready" | "error";
    enabled: boolean;
    error: string | null;
    hasRules: boolean;
    hasRecovery: boolean;
    generation: number;
  };
  refresh(): Promise<void>;
  exportSettings(
    options?: { workspaceNames?: Record<string, string> },
  ): Promise<unknown>;
  previewImportSettings(
    raw: string | object,
    workspaceIds?: readonly string[],
    mapping?: BookmarkImportMapping,
  ): Promise<BookmarkImportPreview>;
  applyImportSettings(
    preview: BookmarkImportPreview,
    mapping?: BookmarkImportMapping,
  ): Promise<BookmarkImportPreview>;
  readRecoverySettings(): Promise<object | null>;
  getOrCreateWorkspaceFolder(
    win: Window,
  ): Promise<{ guid: string; created: boolean; workspaceId: string }>;
  releaseCreatedWorkspaceFolder?(
    folder: { guid: string; created: boolean; workspaceId: string },
  ): Promise<void>;
}

export interface BookmarkImportReviewOptions {
  service: {
    getStatus(): { generation: number };
    previewImportSettings(
      raw: string | object,
      workspaceIds?: readonly string[],
      mapping?: BookmarkImportMapping,
    ): Promise<BookmarkImportPreview>;
    applyImportSettings(
      preview: BookmarkImportPreview,
      mapping?: BookmarkImportMapping,
    ): Promise<BookmarkImportPreview>;
  };
  raw: string | object;
  workspaceIds(): readonly string[];
  isOpen(): boolean;
  map(preview: BookmarkImportPreview): Promise<BookmarkImportMapping | null>;
  confirm(preview: BookmarkImportPreview): Promise<boolean>;
}

export interface BookmarkOperationConfirmation {
  kind: "transfer" | "delete";
  count?: number;
  hiddenCount?: number;
  sourceGuids?: readonly string[];
  targetParentGuid?: string;
  scope?: readonly string[] | null;
}

export interface WorkspaceBookmarksUI {
  manage(): Promise<void>;
  showFolderScope(guid: string): Promise<void>;
  confirmOperation(operation: BookmarkOperationConfirmation): Promise<boolean>;
}

export interface BookmarkEditingWindow extends Window {
  gURLBar?: { controller?: { cancelQuery(): void }; view?: { close(): void } };
  browsingContext: BrowsingContext;
  floorpWorkspaceBookmarksUI?: WorkspaceBookmarksUI;
  gEditItemOverlay?: {
    bookmarkState?: {
      guid?: string;
      readonly parentGuid?: string;
      _parentGuidChanged(guid: string): Promise<void>;
    };
  };
  StarUI?: {
    panel: XULPopupElement;
    readonly selectedWorkspaceBookmarkGuid?: string;
    getWorkspaceBookmarks?(): Promise<ScopedBookmark[]>;
    selectWorkspaceBookmark?(guid: string): Promise<void>;
  };
}

export interface LocalizedBookmarkTitles {
  bookmarks: {
    getLocalizedTitle(info: { guid: string; title: string }): string;
  };
}
