/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import i18next from "i18next";
import {
  getGBrowser,
  type SplitViewTab,
  type SplitViewWrapper,
} from "../data/types.js";
import { splitViewConfig } from "../data/config.js";
import {
  computeDropZone,
  zoneToLayout,
  zoneToTabOrder,
  type DropZone,
} from "../utils/zone-computation.js";
import {
  showDropOverlay,
  hideDropOverlay,
  removeDropOverlay,
} from "./split-view-drop-overlay.js";
import {
  getActiveSplitViewGroupId,
  setPersistedGroupLayout,
} from "../patches/session-restore.js";
import { applyLayout } from "../layout.js";
const TAB_DROP_TYPE = "application/x-moz-tabbrowser-tab";
const NEW_WINDOW_ZONE_ID = "floorp-new-window-drop-zone";

let activeZone: DropZone = "right";
let isTabDragging = false;
/** Tab being dragged, captured at dragstart. */
let draggedTabAtStart: SplitViewTab | null = null;
let cleanupFns: (() => void)[] = [];
let logger: ConsoleInstance | null = null;

const t = (key: string, opts?: Record<string, string>): string =>
  (i18next.t as (k: string, o?: Record<string, string>) => string)(key, opts);

function getTabpanels(): HTMLElement | null {
  return document?.getElementById("tabbrowser-tabpanels") as HTMLElement | null;
}

/**
 * Find any existing split view wrapper by scanning tabs for `splitview`.
 * Works even after Firefox "deactivates" the split view by switching tabs,
 * because the wrapper reference persists on the tab objects.
 */
function findExistingSplitView(): SplitViewWrapper | null {
  const gBrowser = getGBrowser();
  if (!gBrowser) return null;
  if (gBrowser.activeSplitView) return gBrowser.activeSplitView;
  for (const tab of gBrowser.tabs) {
    if (tab.splitview) return tab.splitview as unknown as SplitViewWrapper;
  }
  return null;
}

/**
 * Find the most recently active tab (highest _lastAccessed) that is NOT the
 * dragged tab and NOT already in a split view. Uses Firefox's internal
 * _lastAccessed timestamp — same pattern as gecko-show-previously-selected-tab
 * in mouse-gesture/utils/actions.ts.
 */
function findLastActivePartnerTab(
  gBrowser: NonNullable<ReturnType<typeof getGBrowser>>,
  excludeTab: SplitViewTab,
): SplitViewTab | null {
  let best: SplitViewTab | null = null;
  let bestTime = -1;
  for (const tab of gBrowser.tabs) {
    if (tab === excludeTab) continue;
    if ((tab as SplitViewTab).splitview) continue;
    const accessed = (tab as SplitViewTab & { _lastAccessed?: number })
      ._lastAccessed;
    // Infinity = currently selected tab — most recently active, return immediately
    if (accessed === Infinity) return tab as SplitViewTab;
    if (accessed === undefined || accessed === null) continue;
    if (accessed > bestTime) {
      best = tab as SplitViewTab;
      bestTime = accessed;
    }
  }
  return best;
}

function isTabDrag(event: DragEvent): boolean {
  const types = event.dataTransfer?.types;
  if (!types) return false;
  return Array.from(types).includes(TAB_DROP_TYPE);
}

/** Check if the cursor is over the content area (tabpanels rect). */
function isOverContentArea(event: DragEvent): boolean {
  const tabpanels = getTabpanels();
  if (!tabpanels) return false;
  const rect = tabpanels.getBoundingClientRect();
  return (
    event.clientX >= rect.left &&
    event.clientX <= rect.right &&
    event.clientY >= rect.top &&
    event.clientY <= rect.bottom
  );
}

// --- New window drop zone ---

function getOrCreateNewWindowZone(): HTMLElement | null {
  const existing = document?.getElementById(NEW_WINDOW_ZONE_ID);
  if (existing) return existing as HTMLElement;

  const zone = document?.createElement("div") as HTMLElement;
  if (!zone) return null;
  zone.id = NEW_WINDOW_ZONE_ID;

  const icon = document?.createElement("div") as HTMLElement;
  if (icon) {
    icon.className = "floorp-new-window-zone-icon";
    zone.appendChild(icon);
  }

  const label = document?.createElement("div") as HTMLElement;
  if (label) {
    label.className = "floorp-new-window-zone-label";
    label.textContent = t("splitView.dropZone.newWindow");
    zone.appendChild(label);
  }

  zone.addEventListener("dragover", onNewWindowZoneDragOver);
  zone.addEventListener("dragleave", onNewWindowZoneDragLeave);
  zone.addEventListener("drop", onNewWindowZoneDrop);

  document.documentElement!.appendChild(zone);
  return zone;
}

function showNewWindowZone(): void {
  const zone = getOrCreateNewWindowZone();
  if (zone) {
    zone.setAttribute("drag-active", "true");
    const label = zone.querySelector(".floorp-new-window-zone-label");
    if (label) {
      label.textContent = t("splitView.dropZone.newWindow");
    }
  }
}

function removeNewWindowZone(): void {
  const zone = document?.getElementById(NEW_WINDOW_ZONE_ID);
  if (zone) {
    zone.removeEventListener("dragover", onNewWindowZoneDragOver);
    zone.removeEventListener("dragleave", onNewWindowZoneDragLeave);
    zone.removeEventListener("drop", onNewWindowZoneDrop);
    zone.remove();
  }
}

function onNewWindowZoneDragOver(event: DragEvent): void {
  if (!isTabDrag(event)) return;
  const zone = document?.getElementById(NEW_WINDOW_ZONE_ID);
  if (zone) zone.setAttribute("drag-hover", "true");
}

function onNewWindowZoneDragLeave(): void {
  const zone = document?.getElementById(NEW_WINDOW_ZONE_ID);
  if (zone) zone.removeAttribute("drag-hover");
}

function onNewWindowZoneDrop(event: DragEvent): void {
  event.stopPropagation();
  event.preventDefault();
  const zone = document?.getElementById(NEW_WINDOW_ZONE_ID);
  if (zone) zone.removeAttribute("drag-hover");

  const gBrowser = getGBrowser();
  const draggedTab = draggedTabAtStart;
  if (!gBrowser || !draggedTab) {
    cleanup();
    return;
  }

  // Move the dragged tab to a new window
  cleanup();
  gBrowser.replaceTabWithWindow(draggedTab);
}

// --- Document-level drag handlers (capture phase) ---

/** Check if the event target is inside the new window drop zone. */
function isEventInsideNewWindowZone(event: DragEvent): boolean {
  const zone = document.getElementById(NEW_WINDOW_ZONE_ID);
  if (!zone) return false;
  const target = event.target;
  return target instanceof Node ? zone.contains(target) : false;
}

function onDragOver(event: DragEvent): void {
  const types = event.dataTransfer?.types;
  const hasTabType = types ? Array.from(types).includes(TAB_DROP_TYPE) : false;
  if (!hasTabType) return;

  // Bug 2: Don't intercept events inside the new window zone
  if (isEventInsideNewWindowZone(event)) return;

  const overContent = isOverContentArea(event);
  if (!overContent) {
    // Bug 4: Hide overlay when cursor leaves the content area
    hideDropOverlay();
    return;
  }

  // Prevent default to claim the drop target (prevents detach-to-window)
  event.preventDefault();
  event.dataTransfer!.dropEffect = "move";

  // Bug 3: Set data-floorp-tab-dragging attribute to disable pointer events on content
  const tabpanels = getTabpanels();
  if (tabpanels) {
    tabpanels.setAttribute("data-floorp-tab-dragging", "true");
  }

  if (!isTabDragging) {
    isTabDragging = true;
    showNewWindowZone();
  }

  const draggedTab = draggedTabAtStart;
  const activeSplitView = findExistingSplitView();
  const maxPanes = splitViewConfig().maxPanes;

  if (draggedTab?.splitview) return;
  if (activeSplitView && activeSplitView.tabs.length >= maxPanes) return;

  if (!tabpanels) return;
  const rect = tabpanels.getBoundingClientRect();
  const relX = (event.clientX - rect.left) / rect.width;
  const relY = (event.clientY - rect.top) / rect.height;
  activeZone = computeDropZone(relX, relY);
  showDropOverlay(activeZone);
}

function onDrop(event: DragEvent): void {
  if (!isTabDrag(event)) return;

  // Bug 2: Don't intercept drops inside the new window zone
  if (isEventInsideNewWindowZone(event)) return;

  if (!isOverContentArea(event)) return;

  event.preventDefault();
  event.stopPropagation();

  const gBrowser = getGBrowser();
  if (!gBrowser) {
    cleanup();
    return;
  }

  const draggedTab = draggedTabAtStart;
  if (!draggedTab) {
    cleanup();
    return;
  }

  if (draggedTab.splitview) {
    cleanup();
    return;
  }

  // Save state before cleanup — split view creation is deferred so that
  // Firefox's handle_dragend (finishAnimateTabMove, _resetTabsAfterDrop)
  // completes its cleanup first.  Without this delay, handle_dragend resets
  // drag transforms AFTER we've already moved tabs into the wrapper,
  // corrupting the tab bar layout.
  const zone = activeZone;
  const tab = draggedTab;
  const splitView = findExistingSplitView();
  const maxPanes = splitViewConfig().maxPanes;

  cleanup();

  setTimeout(() => {
    if (tab.splitview) return;

    // Center zone: add pane to existing split view, or create new with default layout
    if (zone === "center") {
      if (splitView && splitView.tabs.length < maxPanes) {
        splitView.addTabs([tab]);
        // When going from 2→3 panes, pick a grid-3pane layout based on the
        // current layout direction so the existing pane arrangement is preserved.
        const newPaneCount = splitView.tabs.length;
        if (newPaneCount === 3) {
          const currentLayout =
            splitView.tabs.length >= 2
              ? /* layout resolved from persisted state */ null
              : null;
          // Default center-drop to grid-3pane-right-main (new tab as main right pane)
          // which is a natural extension of horizontal (side-by-side) layout.
          const gridLayout = currentLayout ?? "grid-3pane-right-main";
          const groupId = getActiveSplitViewGroupId();
          if (groupId) {
            setPersistedGroupLayout(groupId, gridLayout);
          }
          applyLayout(logger!);
        } else if (newPaneCount === 4) {
          const groupId = getActiveSplitViewGroupId();
          if (groupId) {
            setPersistedGroupLayout(groupId, "grid-2x2");
          }
          applyLayout(logger!);
        }
      } else if (!splitView) {
        const partnerTab = findLastActivePartnerTab(gBrowser, tab);
        if (!partnerTab) return;
        const wrapper = gBrowser.addTabSplitView([partnerTab, tab]);
        if (wrapper) {
          applyLayout(logger!);
        }
      }
      return;
    }

    // Edge zones always create a NEW split view with the most recently
    // active tab — they never merge into an existing split view.
    // (Merging into existing is handled by the center zone above.)
    {
      const partnerTab = findLastActivePartnerTab(gBrowser, tab);
      if (!partnerTab) return;
      const layout = zoneToLayout(zone);
      const [first, second] = zoneToTabOrder(zone, partnerTab, tab);
      const wrapper = gBrowser.addTabSplitView([first, second]);
      if (wrapper) {
        const groupId = getActiveSplitViewGroupId();
        if (groupId) {
          setPersistedGroupLayout(groupId, layout);
        }
        applyLayout(logger!);
      }
    }
  }, 0);
}

function onDragEnd(): void {
  cleanup();
  // Belt-and-suspenders: ensure Firefox's "movingtab" attribute is cleared.
  // handle_dragend normally does this, but if the dragged tab was consumed
  // by our drop handler, the attribute may linger on gNavToolbox, which
  // applies `pointer-events: none` to the nav-bar and blocks all clicks
  // (including context menus).
  document.getElementById("tabbrowser-tabs")?.removeAttribute("movingtab");
  document.getElementById("navigator-toolbox")?.removeAttribute("movingtab");
}

/** Capture split view state and dragged tab on dragstart. */
function onDragStart(event: DragEvent): void {
  const types = event.dataTransfer?.types;
  if (!types) return;
  if (!Array.from(types).includes(TAB_DROP_TYPE)) return;
  const gBrowser = getGBrowser();
  if (!gBrowser) return;
  // Bug 5: Try to get the actual dragged tab from the event target.
  // event.target during dragstart on tabContainer is the tab element,
  // which has a linkedTab property pointing to the SplitViewTab.
  const target = event.target;
  if (
    target &&
    typeof target === "object" &&
    "linkedTab" in target &&
    target.linkedTab
  ) {
    draggedTabAtStart = target.linkedTab as SplitViewTab;
  } else {
    // Fallback: use selectedTab (may be incorrect for non-selected tab drags)
    draggedTabAtStart = gBrowser.selectedTab ?? null;
  }
}

function cleanup(): void {
  isTabDragging = false;
  draggedTabAtStart = null;
  activeZone = "right";
  removeDropOverlay();
  removeNewWindowZone();
  // Bug 3: Remove the tab-dragging attribute when drag ends
  const tabpanels = getTabpanels();
  if (tabpanels) {
    tabpanels.removeAttribute("data-floorp-tab-dragging");
  }
}

// --- Public API ---

export function initTabDrop(initLogger: ConsoleInstance): void {
  logger = initLogger;
  const onOver = (e: Event) => onDragOver(e as DragEvent);
  const onDropFn = (e: Event) => onDrop(e as DragEvent);
  const onEnd = () => onDragEnd();
  const onStart = (e: Event) => onDragStart(e as DragEvent);

  // Capture split view state on dragstart (before Firefox switches tabs).
  const tabContainer = getGBrowser()?.tabContainer;
  if (tabContainer) {
    tabContainer.addEventListener("dragstart", onStart);
  }

  // Use capture phase on document to intercept events before they
  // reach the browser content area (which forwards to content process).
  document.addEventListener("dragover", onOver, true);
  document.addEventListener("drop", onDropFn, true);
  document.addEventListener("dragend", onEnd);

  cleanupFns = [
    () => tabContainer?.removeEventListener("dragstart", onStart),
    () => document.removeEventListener("dragover", onOver, true),
    () => document.removeEventListener("drop", onDropFn, true),
    () => document.removeEventListener("dragend", onEnd),
  ];

  logger.debug("[tab-drop] document-level listeners attached (capture phase)");
}

export function destroyTabDrop(): void {
  for (const fn of cleanupFns) fn();
  cleanupFns = [];
  removeDropOverlay();
  removeNewWindowZone();
  cleanup();
}
