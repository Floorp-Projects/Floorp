/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getGBrowser, type SplitViewTab } from "../data/types.js";
import { swapPanesByTab } from "../utils/reorder-panes.js";

let activeDragCleanup: (() => void) | null = null;

function getTabpanels(): HTMLElement | null {
  return document?.getElementById("tabbrowser-tabpanels") as
    | HTMLElement
    | null;
}

function panelIdToTab(panelId: string): SplitViewTab | null {
  const gBrowser = getGBrowser();
  if (!gBrowser?.tabs) {
    return null;
  }
  for (const tab of gBrowser.tabs) {
    if (tab.linkedPanel === panelId) {
      return tab;
    }
  }
  return null;
}

function removeAllGripsFrom(tabpanels: Element | null): void {
  if (!tabpanels) {
    return;
  }
  for (const g of tabpanels.querySelectorAll(".floorp-pane-drag-grip")) {
    g.remove();
  }
}

/**
 * Inserts or refreshes drag grips on each split-view panel (same cadence as split handles).
 */
export function updatePaneDragGrips(
  logger: ConsoleInstance,
  panelIds: string[],
): void {
  const tabpanels = getTabpanels();
  removeAllGripsFrom(tabpanels);

  if (!tabpanels || panelIds.length < 2) {
    return;
  }

  for (const pid of panelIds) {
    const panel = document?.getElementById(pid);
    if (!panel?.classList.contains("split-view-panel")) {
      continue;
    }
    const grip = document?.createXULElement("box");
    if (!grip) {
      continue;
    }
    grip.className = "floorp-pane-drag-grip";
    grip.addEventListener("mousedown", (ev: Event) => {
      onGripMouseDown(ev as MouseEvent, logger);
    });
    panel.insertBefore(grip, panel.firstChild);
  }

  logger.debug(
    `[pane-drag] grips updated for ${panelIds.length} panel(s)`,
  );
}

function onGripMouseDown(e: MouseEvent, logger: ConsoleInstance): void {
  e.preventDefault();
  e.stopPropagation();

  if (activeDragCleanup) {
    activeDragCleanup();
    activeDragCleanup = null;
  }

  const grip = e.currentTarget as unknown as XULElement;
  const panel = grip.parentElement as HTMLElement | null;
  if (!panel?.id) {
    return;
  }

  const sourceTab = panelIdToTab(panel.id);
  if (!sourceTab?.splitview) {
    return;
  }

  const tabpanels = getTabpanels();
  if (!tabpanels) {
    return;
  }

  tabpanels.setAttribute("data-floorp-dragging", "pane-reorder");
  panel.setAttribute("data-floorp-drag-source", "true");

  type RectEntry = { id: string; el: HTMLElement; rect: DOMRect };
  let cachedRects: RectEntry[] = [];
  const refreshRects = (): void => {
    cachedRects = [];
    for (const child of tabpanels.querySelectorAll(".split-view-panel")) {
      const el = child as HTMLElement;
      if (el.id) {
        cachedRects.push({
          id: el.id,
          el,
          rect: el.getBoundingClientRect(),
        });
      }
    }
  };
  refreshRects();

  let lastX = e.clientX;
  let lastY = e.clientY;
  let rafId = 0;
  let pending = false;

  const cleanup = (): void => {
    if (rafId !== 0) {
      cancelAnimationFrame(rafId);
      rafId = 0;
    }
    document?.removeEventListener("mousemove", onMove);
    document?.removeEventListener("mouseup", onUp);
    document?.removeEventListener("keydown", onKey, true);
    tabpanels.removeAttribute("data-floorp-dragging");
    for (const { el } of cachedRects) {
      el.removeAttribute("data-floorp-drag-source");
      el.removeAttribute("data-floorp-drop-target");
    }
    panel.removeAttribute("data-floorp-drag-source");
    activeDragCleanup = null;
  };

  const onMove = (ev: MouseEvent): void => {
    lastX = ev.clientX;
    lastY = ev.clientY;
    if (pending) {
      return;
    }
    pending = true;
    rafId = requestAnimationFrame(() => {
      rafId = 0;
      pending = false;
      const x = lastX;
      const y = lastY;
      let hit: HTMLElement | null = null;
      for (const { el, rect } of cachedRects) {
        if (
          x >= rect.left &&
          x <= rect.right &&
          y >= rect.top &&
          y <= rect.bottom
        ) {
          hit = el;
          break;
        }
      }
      for (const { el } of cachedRects) {
        if (el === hit) {
          el.setAttribute("data-floorp-drop-target", "true");
        } else {
          el.removeAttribute("data-floorp-drop-target");
        }
      }
    });
  };

  const onUp = (ev: MouseEvent): void => {
    const targetEntry = cachedRects.find((r) =>
      r.el.hasAttribute("data-floorp-drop-target"),
    );
    const targetTab = targetEntry
      ? panelIdToTab(targetEntry.id)
      : null;
    if (
      targetTab &&
      sourceTab &&
      targetTab !== sourceTab &&
      targetTab.splitview
    ) {
      swapPanesByTab(logger, sourceTab, targetTab);
    }
    cleanup();
    ev.preventDefault();
  };

  const onKey = (ev: KeyboardEvent): void => {
    if (ev.key === "Escape") {
      cleanup();
    }
  };

  activeDragCleanup = cleanup;
  document?.addEventListener("mousemove", onMove);
  document?.addEventListener("mouseup", onUp);
  document?.addEventListener("keydown", onKey, true);
}

export function initPaneDrag(_logger: ConsoleInstance): void {
  /* Grips are created by updatePaneDragGrips; drag uses document listeners per interaction. */
}

export function destroyPaneDrag(): void {
  if (activeDragCleanup) {
    activeDragCleanup();
    activeDragCleanup = null;
  }
  removeAllGripsFrom(getTabpanels());
  const tabpanels = getTabpanels();
  tabpanels?.removeAttribute("data-floorp-dragging");
  for (const el of document?.querySelectorAll(
    ".split-view-panel[data-floorp-drag-source], .split-view-panel[data-floorp-drop-target]",
  ) ?? []) {
    el.removeAttribute("data-floorp-drag-source");
    el.removeAttribute("data-floorp-drop-target");
  }
}
