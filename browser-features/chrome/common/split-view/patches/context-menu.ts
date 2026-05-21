/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import type { SplitViewTab } from "../data/types.js";
import { getGBrowser, getTabContextMenu } from "../data/types.js";
import { splitViewConfig } from "../data/config.js";
import { swapPanesByTab } from "../utils/reorder-panes.js";

const t = (key: string, opts?: Record<string, string>): string =>
  (i18next.t as (k: string, o?: Record<string, string>) => string)(key, opts);

/**
 * Adds "Open in Split View", "Add Pane to Split View" and "Move to Pane"
 * items to the tab context menu.
 */
export function initContextMenu(logger: ConsoleInstance): () => void {
  const tabContainer = getGBrowser()?.tabContainer;
  if (!tabContainer) return () => {};

  const updateLabels = (): void => {
    const openInSplitItem = document?.getElementById("floorp_openInSplitView");
    if (openInSplitItem) {
      openInSplitItem.setAttribute("label", t("splitView.contextMenu.openInSplitView"));
    }
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

  const onTabContextMenu = (): void => {
    const separateItem = document?.getElementById("context_separateSplitView");
    if (!separateItem) return;

    const gBrowser = getGBrowser();
    const splitViewEnabled = Services.prefs.getBoolPref(
      "browser.tabs.splitView.enabled",
      false,
    );
    if (!splitViewEnabled) return;

    const activeSplitView = gBrowser?.activeSplitView;
    const contextTabs: SplitViewTab[] = getTabContextMenu()?.contextTabs ?? [];
    const hasSplitViewTab = contextTabs.some(
      (tab: SplitViewTab) => tab.splitview,
    );

    // gBrowser.selectedTabs reflects the current multi-selection state
    // regardless of TabContextMenu timing.  It always contains at least
    // the active tab; length >= 2 means the user Ctrl/Shift-clicked.
    const multiSelectedTabs: SplitViewTab[] = gBrowser?.selectedTabs ?? [];

    logger.debug(
      `[contextMenu] activeSplitView=${!!activeSplitView}, ` +
        `contextTabs=${contextTabs.length}, hasSplitViewTab=${hasSplitViewTab}, ` +
        `multiSelected=${multiSelectedTabs.length}, ` +
        `activeTabs=${activeSplitView?.tabs?.length ?? 0}`,
    );

    // === Open in Split View (create new split from multi-selected tabs) ===
    const shouldShowOpenInSplit =
      multiSelectedTabs.length >= 2 && !activeSplitView;

    let openInSplitItem = document?.getElementById(
      "floorp_openInSplitView",
    ) as unknown as XULElement | null;

    if (shouldShowOpenInSplit) {
      if (!openInSplitItem) {
        openInSplitItem = document?.createXULElement("menuitem") as unknown as XULElement;
        if (openInSplitItem) {
          openInSplitItem.id = "floorp_openInSplitView";
          openInSplitItem.setAttribute(
            "label",
            t("splitView.contextMenu.openInSplitView"),
          );
          openInSplitItem.addEventListener("command", () => {
            const currentGBrowser = getGBrowser();
            if (!currentGBrowser) return;
            const currentSelectedTabs = currentGBrowser.selectedTabs;
            const maxPanes = splitViewConfig.value.maxPanes;
            const tabsToSplit = currentSelectedTabs.slice(0, maxPanes);
            logger.debug(
              `[contextMenu:command] opening ${tabsToSplit.length} tab(s) in new split view`,
            );
            if (tabsToSplit.length >= 2) {
              currentGBrowser.addTabSplitView(tabsToSplit);
            }
          });
          separateItem.after(openInSplitItem);
        }
      }
      if (openInSplitItem) {
        openInSplitItem.hidden = false;
      }
    } else if (openInSplitItem) {
      openInSplitItem.hidden = true;
    }

    // === Add Pane to Split View ===
    const shouldShowAddPane =
      hasSplitViewTab &&
      activeSplitView &&
      activeSplitView.tabs.length < splitViewConfig.value.maxPanes;

    let addPaneItem = document?.getElementById(
      "floorp_addPaneToSplitView",
    ) as unknown as XULElement | null;

    if (shouldShowAddPane) {
      if (!addPaneItem) {
        addPaneItem = document?.createXULElement("menuitem") as unknown as XULElement;
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
          const insertAddPaneAfter =
            openInSplitItem && !openInSplitItem.hidden
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
    const shouldShowMoveToPane =
      hasSplitViewTab &&
      activeSplitView &&
      activeSplitView.tabs.length >= 2;

    let moveMenu = document?.getElementById(
      "floorp_moveTabToPane",
    ) as unknown as XULElement | null;

    if (shouldShowMoveToPane) {
      if (!moveMenu) {
        moveMenu = document?.createXULElement("menu") as unknown as XULElement;
        if (moveMenu) {
          moveMenu.id = "floorp_moveTabToPane";
          moveMenu.setAttribute(
            "label",
            t("splitView.contextMenu.moveToPane"),
          );

          const popup = document?.createXULElement(
            "menupopup",
          ) as unknown as XULElement;
          if (popup) {
            popup.id = "floorp_moveTabToPanePopup";
            popup.addEventListener("popupshowing", () => {
              onMoveToPanePopupShowing(logger);
            });
            moveMenu.appendChild(popup);
          }

          const insertMoveAfter =
            addPaneItem && !addPaneItem.hidden
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

  tabContainer.addEventListener("contextmenu", onTabContextMenu);
  logger.debug("[patch] context menu listener attached");

  return () => {
    tabContainer.removeEventListener("contextmenu", onTabContextMenu);
  };
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

  const contextTabs: SplitViewTab[] =
    getTabContextMenu()?.contextTabs ?? [];
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

    const item = document?.createXULElement("menuitem") as unknown as XULElement;
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
