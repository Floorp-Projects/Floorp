// SPDX-License-Identifier: MPL-2.0

type BookmarkItem = Element & { _placesNode?: unknown };
type PlacesToolbar = Element & { _placesView?: unknown };
type BookmarkWindow = Window & {
  BookmarksEventHandler?: {
    onClick(event: MouseEvent, view: unknown): void;
    onCommand(event: Event): void;
  };
};

/** Menu items open through command, not the toolbar button click handler. */
export function handleBookmarkMenuMiddleCommand(
  event: Event,
  bookmarkBar: Element,
): boolean {
  const command = event as Event & {
    button?: number;
    originalTarget?: EventTarget | null;
  };
  const target = command.originalTarget;
  const win = bookmarkBar.ownerDocument.defaultView as BookmarkWindow | null;
  if (
    event.type !== "command" || command.button !== 1 ||
    event.defaultPrevented ||
    !(target instanceof Element) || !target.matches("menuitem.bookmark-item") ||
    !bookmarkBar.contains(target) || !(target as BookmarkItem)._placesNode ||
    !win?.BookmarksEventHandler
  ) {
    return false;
  }

  // XUL commands retain the middle button. The layout's generic command
  // fallback only checks modifiers and would replace the current page.
  // Preserve the native destination, background preference and private state.
  win.BookmarksEventHandler.onCommand(event);
  event.preventDefault();
  event.stopImmediatePropagation();
  return true;
}

/** Preserve the original Places target when bookmarks leave the toolbox. */
export function handleBookmarkMiddleClick(
  event: Event,
  bookmarkBar: Element,
): boolean {
  if (
    event.type !== "click" || !(event instanceof MouseEvent) ||
    event.button !== 1 || event.defaultPrevented
  ) {
    return false;
  }

  const target = (event as MouseEvent & { originalTarget: EventTarget | null })
    .originalTarget;
  if (
    !(target instanceof Element) ||
    !target.matches("toolbarbutton.bookmark-item") ||
    !bookmarkBar.contains(target) || !(target as BookmarkItem)._placesNode
  ) {
    return false;
  }
  const toolbar = target.closest("#PlacesToolbar") as PlacesToolbar | null;
  const win = bookmarkBar.ownerDocument.defaultView as BookmarkWindow | null;
  if (!toolbar?._placesView || !win?.BookmarksEventHandler) {
    return false;
  }

  // Redispatching a clone on navigator-toolbox replaces target/originalTarget.
  // Use Places' native handling for bookmarks and folders, retaining modifiers
  // and its foreground/background policy. Consume the original once: this
  // forwarder also listens on document in the capture phase.
  win.BookmarksEventHandler.onClick(event, toolbar._placesView);
  event.preventDefault();
  event.stopImmediatePropagation();
  return true;
}
