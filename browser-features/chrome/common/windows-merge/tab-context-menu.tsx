// SPDX-License-Identifier: MPL-2.0

/**
 * Tab context menu entry: "Merge All Windows", placed directly under
 * "Select All Tabs" in the native tab context menu.
 */

import { createRootHMR, render } from "@nora/solid-xul";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { getActionDisplayName } from "#features-chrome/common/mouse-gesture/utils/gestures.ts";
import { createSignal } from "solid-js";
import { MERGE_WINDOWS_ACTION_ID, mergeAllWindows } from "./controller.ts";

export const MERGE_ALL_WINDOWS_MENU_ID = "context_floorpMergeAllWindows";

/**
 * Floorp-owned marker: the context-menu customizer uses it to identify the
 * item and to keep the user's ordering/hiding decisions across updates.
 */
export const MERGE_ALL_WINDOWS_MENU_KEY = "floorp.windows-merge.all-windows";

/** Anchor the entry is inserted after. */
const SELECT_ALL_TABS_ID = "context_selectAllTabs";

export function MergeAllWindowsMenuItem() {
  const [label, setLabel] = createSignal(
    getActionDisplayName(MERGE_WINDOWS_ACTION_ID),
  );

  createRootHMR(() => {
    addI18nObserver(() =>
      setLabel(getActionDisplayName(MERGE_WINDOWS_ACTION_ID))
    );
  }, import.meta.hot);

  return (
    <xul:menuitem
      id={MERGE_ALL_WINDOWS_MENU_ID}
      data-floorp-context-menu-key={MERGE_ALL_WINDOWS_MENU_KEY}
      label={label()}
      onCommand={() => mergeAllWindows(window)}
    />
  );
}

/**
 * Mounts the entry into `popup` right after "Select All Tabs".
 *
 * Returns a disposer that removes the entry again — or a no-op when the popup
 * is missing or the entry is already mounted.
 */
export function installMergeAllWindowsMenuItem(
  popup: Element | null | undefined,
): () => void {
  if (!popup) {
    console.error("[windows-merge] #tabContextMenu not found; no menu entry");
    return () => {};
  }

  // A leftover entry means an earlier mount was not disposed (HMR reload):
  // drop it so the fresh component owns the wiring and the locale observer.
  popup.querySelector(`#${MERGE_ALL_WINDOWS_MENU_ID}`)?.remove();

  // The entry belongs *below* "Select All Tabs", so it is rendered before the
  // anchor's next sibling. Falls back to appending when the anchor is gone.
  const anchor = popup.querySelector(`#${SELECT_ALL_TABS_ID}`);
  const nextSibling = anchor?.parentElement === popup
    ? anchor.nextElementSibling
    : null;

  // A dedicated root per mount: `render` attaches its DOM cleanup to the
  // enclosing root, so the caller needs a disposer of its own.
  let disposeRoot: (() => void) | null = null;
  try {
    createRootHMR((dispose) => {
      render(MergeAllWindowsMenuItem, popup, {
        marker: nextSibling ?? undefined,
      });
      disposeRoot = dispose;
    }, import.meta.hot);
  } catch (error) {
    console.error("[windows-merge] could not add the tab menu entry:", error);
    return () => {};
  }

  return () => {
    disposeRoot?.();
    disposeRoot = null;
    // Removing the node explicitly keeps the menu clean even when the render
    // root's own DOM cleanup cannot resolve the inserted node.
    popup.querySelector(`#${MERGE_ALL_WINDOWS_MENU_ID}`)?.remove();
  };
}
