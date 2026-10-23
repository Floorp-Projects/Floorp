// SPDX-License-Identifier: MPL-2.0

import type {
  BookmarkVisibilityContext,
  WorkspaceBookmarksService,
} from "./types.ts";

export const BOOKMARK_SCOPES_CHANGED = "floorp.workspace-bookmarks.changed";

export function getWorkspaceBookmarks(): WorkspaceBookmarksService {
  return (ChromeUtils.importESModule(
    "resource://noraneko/modules/WorkspaceBookmarks.sys.mjs",
  ) as { WorkspaceBookmarks: WorkspaceBookmarksService }).WorkspaceBookmarks;
}

export function isBookmarkContextCurrent(
  win: Window,
  context: BookmarkVisibilityContext,
): boolean {
  if (win.closed) return false;
  const current = getWorkspaceBookmarks().getContext(win);
  return current.generation === context.generation &&
    current.windowGeneration === context.windowGeneration &&
    current.windowId === context.windowId &&
    current.workspaceId === context.workspaceId &&
    current.enabled === context.enabled &&
    current.scopeEnabled === context.scopeEnabled &&
    current.ready === context.ready;
}
