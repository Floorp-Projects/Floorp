/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import type { SplitViewTab } from "../data/types.js";
import { getGBrowser, getTabContextMenu } from "../data/types.js";
import { splitViewConfig } from "../data/config.js";
import { swapPanesByTab } from "../utils/reorder-panes.js";
import { canOpenContextTabsInSplitView } from "../utils/context-menu-policy.ts";

const t = (key: string, opts?: Record<string, string>): string =>
  (i18next.t as (k: string, o?: Record<string, string>) => string)(key, opts);

/**
 * Extends the native split-view entry and adds pane management actions.
 */
export function initContextMenu(logger: ConsoleInstance): void {
  const tabMenu = document?.getElementById("tabContextMenu");
  const openInSplitItem = document?.getElementById(
    "context_moveTabToSplitView",
  ) as XULElement | null;
  if (!tabMenu || !openInSplitItem) return;

  // Remove the former duplicate when this module is hot-reloaded.
  document?.getElementById("floorp_openInSplitView")?.remove();

  const updateLabels = (): void => {
    const addPaneItem = document?.getElementById("floorp_addPaneToSplitView");
    if (addPaneItem) {
      addPaneItem.setAttribute("label", t("splitView.contextMenu.addPane"));
    }
    const moveMenu = document?.getElementById("floorp_moveTabToPane");
    if (moveMenu) {
      moveMenu.setAttribute("label", t("splitView.contextMenu.moveToPane"));
    }
  };

  addI18nObserver(updateLabels);

  const onTabContextMenu = (event: Event): void => {
    if (event.target !== tabMenu) return;
    const separateItem = document?.getElementById("context_separateSplitView");
    if (!separateItem) return;

    const gBrowser = getGBrowser();
    // Native popupshowing runs first and resolves the right-clicked tab's
    // contextTabs. Do not substitute a selection from elsewhere in the strip.
    const contextTabs = getTabContextMenu()?.contextTabs ?? [];
    const maxPanes = splitViewConfig().maxPanes;
    openInSplitItem.removeAttribute("tooltiptext");
    const splitViewEnabled = Services.prefs.getBoolPref(
      "browser.tabs.splitView.enabled",
      false,
    );
    if (!splitViewEnabled) return;

    if (canOpenContextTabsInSplitView(contextTabs, maxPanes)) {
      openInSplitItem.removeAttribute("disabled");
    } else {
      openInSplitItem.setAttribute("disabled", "true");
    }
    if (!openInSplitItem.hidden && contextTabs.length > maxPanes) {
      // Fluent asynchronously clears attributes absent from its translation.
      // Own the label while over the limit so the explanation stays visible.
      // The native updater restores its l10n ID the next time the menu opens.
      openInSplitItem.removeAttribute("data-l10n-id");
      openInSplitItem.setAttribute(
        "label",
        t("splitView.contextMenu.paneLimit", {
          max: String(maxPanes),
        }),
      );
    }

    const activeSplitView = gBrowser?.activeSplitView;
    const hasSplitViewTab = contextTabs.some(
      (tab: SplitViewTab) => tab.splitview,
    );

    logger.debug(
      `[contextMenu] activeSplitView=${!!activeSplitView}, ` +
        `contextTabs=${contextTabs.length}, hasSplitViewTab=${hasSplitViewTab}, ` +
        `activeTabs=${activeSplitView?.tabs?.length ?? 0}`,
    );

    // === Add Pane to Split View ===
    const shouldShowAddPane = hasSplitViewTab &&
      activeSplitView &&
      activeSplitView.tabs.length < splitViewConfig().maxPanes;

    let addPaneItem = document?.getElementById(
      "floorp_addPaneToSplitView",
    ) as XULElement | null;

    if (shouldShowAddPane) {
      if (!addPaneItem) {
        addPaneItem = document?.createXULElement("menuitem") as XULElement;
        if (addPaneItem) {
          addPaneItem.id = "floorp_addPaneToSplitView";
          addPaneItem.setAttribute(
            "label",
            t("splitView.contextMenu.addPane"),
          );
          addPaneItem.addEventListener("command", () => {
            const currentGBrowser = getGBrowser();
            const currentSplitView = currentGBrowser?.activeSplitView;
            const currentContextTabs: SplitViewTab[] =
              getTabContextMenu()?.contextTabs ?? [];
            const nonSplitTabs = currentContextTabs.filter(
              (tab: SplitViewTab) => !tab.splitview,
            );
            logger.debug(
              `[contextMenu:command] adding ${nonSplitTabs.length} tab(s) to split view`,
            );
            if (currentSplitView && nonSplitTabs.length > 0) {
              currentSplitView.addTabs(nonSplitTabs);
            }
          });
          const insertAddPaneAfter = openInSplitItem && !openInSplitItem.hidden
            ? openInSplitItem
            : separateItem;
          insertAddPaneAfter.after(addPaneItem);
        }
      }
      if (addPaneItem) {
        addPaneItem.hidden = false;
      }
    } else if (addPaneItem) {
      addPaneItem.hidden = true;
    }

    // === Move to Pane submenu ===
    const shouldShowMoveToPane = hasSplitViewTab &&
      activeSplitView &&
      activeSplitView.tabs.length >= 2;

    let moveMenu = document?.getElementById(
      "floorp_moveTabToPane",
    ) as XULElement | null;

    if (shouldShowMoveToPane) {
      if (!moveMenu) {
        moveMenu = document?.createXULElement("menu") as XULElement;
        if (moveMenu) {
          moveMenu.id = "floorp_moveTabToPane";
          moveMenu.setAttribute(
            "label",
            t("splitView.contextMenu.moveToPane"),
          );

          const popup = document?.createXULElement(
            "menupopup",
          ) as XULElement;
          if (popup) {
            popup.id = "floorp_moveTabToPanePopup";
            popup.addEventListener("popupshowing", () => {
              onMoveToPanePopupShowing(logger);
            });
            moveMenu.appendChild(popup);
          }

          const insertMoveAfter = addPaneItem && !addPaneItem.hidden
            ? addPaneItem
            : openInSplitItem && !openInSplitItem.hidden
            ? openInSplitItem
            : separateItem;
          insertMoveAfter.after(moveMenu);
        }
      }
      if (moveMenu) {
        moveMenu.hidden = false;
        moveMenu.setAttribute(
          "label",
          t("splitView.contextMenu.moveToPane"),
        );
      }
    } else if (moveMenu) {
      moveMenu.hidden = true;
    }
  };

  // Keep the native command: it opens the partner picker for one tab and
  // preserves placement/order for multiple tabs. Guard again at activation
  // in case the selection, limit or feature preference changed while open.
  const onOpenCommand = (event: Event): void => {
    if (
      !Services.prefs.getBoolPref("browser.tabs.splitView.enabled", false) ||
      !canOpenContextTabsInSplitView(
        getTabContextMenu()?.contextTabs ?? [],
        splitViewConfig().maxPanes,
      )
    ) {
      event.preventDefault();
      event.stopImmediatePropagation();
    }
  };
  tabMenu.addEventListener("popupshowing", onTabContextMenu);
  // Capture also runs before native handlers attached directly to the item.
  openInSplitItem.addEventListener("command", onOpenCommand, true);
  onCleanup(() => {
    tabMenu.removeEventListener("popupshowing", onTabContextMenu);
    openInSplitItem.removeEventListener("command", onOpenCommand, true);
    openInSplitItem.removeAttribute("tooltiptext");
  });
  logger.debug("[patch] context menu listener attached");
}

// ===== Move to Pane helpers =====

function onMoveToPanePopupShowing(logger: ConsoleInstance): void {
  const popup = document?.getElementById("floorp_moveTabToPanePopup");
  if (!popup) return;

  // Clear previous items
  while (popup.lastChild) {
    popup.removeChild(popup.lastChild);
  }

  const gBrowser = getGBrowser();
  const activeSplitView = gBrowser?.activeSplitView;
  if (!activeSplitView) return;

  const contextTabs: SplitViewTab[] = getTabContextMenu()?.contextTabs ?? [];
  const contextTab = contextTabs[0];
  if (!contextTab) return;

  const splitTabs = activeSplitView.tabs;
  const currentIndex = splitTabs.indexOf(
    contextTab as SplitViewTab,
  );
  if (currentIndex === -1) return;

  for (let i = 0; i < splitTabs.length; i++) {
    if (i === currentIndex) continue;

    const targetTab = splitTabs[i];
    const tabTitle = truncateTitle(
      targetTab.label || `Tab ${i + 1}`,
      30,
    );

    const item = document?.createXULElement("menuitem") as XULElement;
    if (!item) continue;

    item.setAttribute(
      "label",
      t("splitView.contextMenu.moveToPaneN", {
        n: String(i + 1),
        title: tabTitle,
      }),
    );
    // Capture tab references (not indices) to avoid stale closure issues
    // if tabs are reordered between popup showing and command execution.
    const fromTab = contextTab;
    const toTab = targetTab;
    item.addEventListener("command", () => {
      swapPanesByTab(logger, fromTab, toTab);
    });
    popup.appendChild(item);
  }
}

function truncateTitle(title: string, maxLen: number): string {
  return title.length > maxLen
    ? `${title.substring(0, maxLen - 1)}\u2026`
    : title;
}
