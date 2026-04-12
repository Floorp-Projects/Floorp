/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { SplitViewLayout, SplitViewTab } from "../data/types.js";
import { getGBrowser } from "../data/types.js";
import { clearSplitHandles } from "../components/split-view-splitters.js";
import { clearGridStyles } from "../layout.js";
import type { PatchState } from "./patch-state.js";
import {
  applySplitViewSessionMarkersForTabs,
  resolveLayoutForPanelIds,
} from "./session-restore.js";
import {
  ensureSplitPanelsActiveClassFromState,
  refreshActiveSplitPaneIndicator,
} from "./active-pane-tracker.js";
import { resetSplitPanelPresentationState } from "../utils/reset-split-panel-presentation.js";
import {
  ensureSplitPaneTabBrowsersAreWarmed,
  scheduleSplitPaneWarmRetries,
} from "./activate-split-pane-browsers.js";

export interface TabpanelsPatchResult {
  unpatch(): void;
}

/**
 * Monkey-patches MozTabpanels to support N-pane split view:
 * - splitViewPanels setter: ensures all panels get active class, cleans stale panels, applies layout
 * - isSplitViewActive setter: manages floorp attributes and cleanup on deactivation
 * - showSplitViewPanels: filters out tabs with destroyed browsers
 */
export function patchTabpanels(
  logger: ConsoleInstance,
  state: PatchState,
  onPanelsChanged: (panelIds: string[], layout: SplitViewLayout) => void,
): TabpanelsPatchResult | null {
  const gBrowser = getGBrowser();
  if (!gBrowser?.tabpanels) {
    logger.warn("[patch] gBrowser.tabpanels not available, skipping patch");
    return null;
  }

  const tabpanels = gBrowser.tabpanels;
  const proto = Object.getPrototypeOf(tabpanels);

  const syncSplitTabMarkerAttrs = (): number => {
    const tabs = document?.querySelectorAll<HTMLElement>(
      "#tabbrowser-tabs .tabbrowser-tab",
    );
    if (!tabs) {
      return 0;
    }

    let splitTabCount = 0;
    for (const tab of tabs) {
      const hasSplit = Boolean(
        (tab as unknown as { splitview?: unknown }).splitview,
      );
      if (hasSplit) {
        splitTabCount++;
        tab.setAttribute("data-floorp-split-tab", "true");
      } else {
        tab.removeAttribute("data-floorp-split-tab");
      }
    }
    return splitTabCount;
  };

  // --- Track originals for unpatch ---
  let patchedSplitViewPanels = false;
  let patchedIsSplitViewActive = false;
  let origShowSplitViewPanels: ((tabs: SplitViewTab[]) => void) | null = null;

  // --- Patch splitViewPanels setter ---
  const origPanelsDesc = Object.getOwnPropertyDescriptor(
    proto,
    "splitViewPanels",
  );

  if (origPanelsDesc?.set && origPanelsDesc?.get) {
    patchedSplitViewPanels = true;

    Object.defineProperty(tabpanels, "splitViewPanels", {
      set(newPanels: string[]) {
        // Always call the original setter to ensure upstream state:
        // - .split-view-panel class on panels
        // - column attributes
        // - click/mouseover/mouseout event listeners on browserContainer
        try {
          origPanelsDesc.set!.call(this, newPanels);
        } catch (e) {
          logger.error(
            `[patch:splitViewPanels.set] original setter threw: ${e}`,
          );
        }

        const splitTabCount = syncSplitTabMarkerAttrs();

        // Grid cell placement follows `column="0"`…`"3"`. After tab reorder,
        // keep attributes aligned with splitViewPanels order.
        if (
          newPanels.length === 4 &&
          (this as Element).getAttribute("split-view-layout") === "grid-2x2"
        ) {
          const root = this as HTMLElement;
          for (let i = 0; i < 4; i++) {
            const id = newPanels[i]!;
            const child = root.querySelector(`#${CSS.escape(id)}`);
            if (child) {
              child.setAttribute("column", String(i));
            }
          }
        }

        // Ensure ALL split-view panels have split-view-panel-active class.
        // Also clean up stale classes from panels NOT in the current split view.
        const currentPanelSet = new Set(newPanels);
        const tabpanelsEl = this as HTMLElement;
        for (const child of tabpanelsEl.children) {
          const childId = child.id;
          if (currentPanelSet.has(childId)) {
            if (!child.classList.contains("split-view-panel-active")) {
              child.classList.add("split-view-panel-active");
            }
          } else {
            if (child.classList.contains("split-view-panel")) {
              child.classList.remove("split-view-panel");
              child.removeAttribute("column");
              logger.debug(
                `[patch:splitViewPanels.set] cleaned stale .split-view-panel from ${childId}`,
              );
            }
            if (child.classList.contains("split-view-panel-active")) {
              child.classList.remove("split-view-panel-active");
            }
          }
        }

        // Re-entrancy guard
        if (state.inSplitViewPanelsSet) {
          logger.warn(
            `[patch:splitViewPanels.set] skipped (re-entrant); incoming=[${newPanels.join(", ")}]`,
          );
          return;
        }

        // Skip if panels haven't changed
        const panelKey = newPanels.join(",");
        if (panelKey === state.lastPanelIds) {
          logger.debug(
            `[patch:splitViewPanels.set] skip unchanged panelKey=[${panelKey}]`,
          );
          return;
        }

        state.inSplitViewPanelsSet = true;
        state.lastPanelIds = panelKey;
        logger.debug(
          `[patch:splitViewPanels.set] panels=${newPanels.length}, ids=[${newPanels.join(", ")}]`,
        );

        // Floorp enhancement: update handles and layout
        if (newPanels.length >= 2) {
          this.setAttribute("data-floorp-split", "true");
          // Ensure multibar is set for Lepton theme compatibility
          const tabsToolbar = document?.getElementById("TabsToolbar");
          if (tabsToolbar) {
            if (!tabsToolbar.hasAttribute("multibar")) {
              tabsToolbar.setAttribute("multibar", "true");
              state.multibarSetBySplitView = true;
            }
            tabsToolbar.setAttribute("splitview-multibar", "true");
          }
          const layout = resolveLayoutForPanelIds(newPanels);
          onPanelsChanged(newPanels, layout);
        } else {
          // Split view panels cleared — clean up splitview-multibar
          const tabsToolbar = document?.getElementById("TabsToolbar");
          if (tabsToolbar) {
            const hasSplitTabs = splitTabCount > 0;
            if (hasSplitTabs) {
              if (!tabsToolbar.hasAttribute("multibar")) {
                tabsToolbar.setAttribute("multibar", "true");
                state.multibarSetBySplitView = true;
              }
              tabsToolbar.setAttribute("splitview-multibar", "true");
            } else {
              tabsToolbar.removeAttribute("splitview-multibar");
              if (state.multibarSetBySplitView) {
                tabsToolbar.removeAttribute("multibar");
                state.multibarSetBySplitView = false;
              }
            }
          }
        }
        state.inSplitViewPanelsSet = false;
      },
      get() {
        return origPanelsDesc.get!.call(this);
      },
      configurable: true,
    });
    logger.debug("[patch] splitViewPanels setter/getter patched");
  } else {
    logger.warn("[patch] splitViewPanels descriptor not found on prototype");
  }

  // --- Patch setSplitViewActive method ---
  // Firefox uses setSplitViewActive(updatedValue) method on MozTabpanels
  // (not an isSplitViewActive property setter) to toggle split view state.
  const origSetSplitViewActive = proto.setSplitViewActive as
    | ((updatedValue: boolean) => void)
    | undefined;

  if (typeof origSetSplitViewActive === "function") {
    patchedIsSplitViewActive = true;

    (
      tabpanels as unknown as {
        setSplitViewActive: (updatedValue: boolean) => void;
      }
    ).setSplitViewActive = function (this: HTMLElement, updatedValue: boolean) {
      // Reproduce the native logic: isActive is true only when the
      // selected tab actually belongs to a split view AND the caller
      // requested activation.
      const gBrowserRef = getGBrowser();
      const selectedSplitview = (
        gBrowserRef?.selectedTab as { splitview?: unknown } | undefined
      )?.splitview;
      const isActive = !!selectedSplitview && !!updatedValue;
      logger.debug(
        `[patch:setSplitViewActive] updatedValue=${updatedValue} selectedHasSplitview=${!!selectedSplitview} → isActive=${isActive}`,
      );

      try {
        origSetSplitViewActive.call(this, updatedValue);
      } catch (e) {
        logger.error(`[patch:setSplitViewActive] original threw: ${e}`);
      }

      const splitTabCount = syncSplitTabMarkerAttrs();

      const tabsToolbar = document?.getElementById("TabsToolbar");

      if (isActive) {
        this.setAttribute("data-floorp-split", "true");
        // Enable multibar attribute so Lepton theme doesn't
        // apply negative margins that hide split-view tabs.
        // Record whether multibar was already set (by multirow tabs)
        // so we don't remove it on deactivation.
        if (tabsToolbar) {
          if (!tabsToolbar.hasAttribute("multibar")) {
            tabsToolbar.setAttribute("multibar", "true");
            state.multibarSetBySplitView = true;
          }
          tabsToolbar.setAttribute("splitview-multibar", "true");
        }
      } else {
        this.removeAttribute("data-floorp-split");
        this.removeAttribute("split-view-layout");
        this.removeAttribute("data-floorp-dragging");
        clearSplitHandles();
        clearGridStyles(this);
        // When leaving split view, Gecko can keep `.split-view-panel`
        // / `.deck-selected` on old panes until the next native refresh.
        // Those stale classes can mask the newly selected normal tab panel.
        for (const child of (this as HTMLElement).children) {
          resetSplitPanelPresentationState(child);
        }
        // Clean up active pane indicator
        const staleActivePanes = this.querySelectorAll(
          "[data-floorp-active-pane]",
        );
        for (const el of staleActivePanes) {
          el.removeAttribute("data-floorp-active-pane");
        }
        // Reset panel cache so next activation re-applies layout
        state.lastPanelIds = "";
        // Only remove splitview-multibar / multibar when no split
        // panels remain. The tab group can still exist in the tab bar
        // even after setSplitViewActive is called with false.
        const panels = (this as unknown as { splitViewPanels?: string[] })
          .splitViewPanels;
        const hasPanels = panels && panels.length >= 2;
        const hasSplitTabs = splitTabCount > 0;
        if (hasSplitTabs && tabsToolbar) {
          if (!tabsToolbar.hasAttribute("multibar")) {
            tabsToolbar.setAttribute("multibar", "true");
            state.multibarSetBySplitView = true;
          }
          tabsToolbar.setAttribute("splitview-multibar", "true");
        } else if (!hasPanels && tabsToolbar) {
          tabsToolbar.removeAttribute("splitview-multibar");
          if (state.multibarSetBySplitView) {
            tabsToolbar.removeAttribute("multibar");
            state.multibarSetBySplitView = false;
          }
        }
      }
    };
    logger.debug("[patch] setSplitViewActive method patched");
  } else {
    logger.warn("[patch] setSplitViewActive method not found on prototype");
  }

  // --- Patch showSplitViewPanels ---
  if (typeof gBrowser.showSplitViewPanels === "function") {
    origShowSplitViewPanels = gBrowser.showSplitViewPanels.bind(gBrowser);
    gBrowser.showSplitViewPanels = (tabs: SplitViewTab[]) => {
      if (state.inShowSplitViewPanels) {
        logger.warn(
          `[patch:showSplitViewPanels] skipped (re-entrant); argTabs=${tabs.length} ` +
            `linkedPanels=[${tabs.map((t) => t?.linkedPanel ?? "?").join(", ")}]`,
        );
        return;
      }

      // The wrapper's private #tabs field can become stale after pane
      // swaps (reorderSplitTabsForDesiredOrder updates DOM order but
      // cannot touch the private field).  When #activate() later calls
      // showSplitViewPanels(this.#tabs), the stale order would revert
      // the swap.  Fix: prefer the wrapper's public `tabs` getter
      // (DOM child order) which is always kept in sync by moveTabBefore.
      const wrapper = gBrowser.activeSplitView;
      if (wrapper) {
        const domTabs = wrapper.tabs as SplitViewTab[];
        if (domTabs.length === tabs.length && domTabs.length >= 2) {
          const passedSet = new Set(tabs);
          if (domTabs.every((t: SplitViewTab) => passedSet.has(t))) {
            if (domTabs.some((t: SplitViewTab, i: number) => t !== tabs[i])) {
              logger.debug(
                `[patch:showSplitViewPanels] corrected stale tab order to DOM order: ` +
                  `passed=[${tabs.map((t) => t.linkedPanel).join(", ")}] ` +
                  `dom=[${domTabs.map((t) => t.linkedPanel).join(", ")}]`,
              );
            }
            tabs = domTabs;
          }
        }
      }

      const validTabs = tabs.filter(
        (tab: SplitViewTab) => tab && tab.linkedBrowser,
      );
      const invalidCount = tabs.length - validTabs.length;
      if (invalidCount > 0) {
        logger.warn(
          `[patch:showSplitViewPanels] filtered out ${invalidCount} tab(s) with null linkedBrowser`,
        );
      }
      const inputDetail = tabs
        .map(
          (t, i) =>
            `[${i}]=${t?.linkedPanel ?? "null"}:${t?.label?.slice(0, 24) ?? ""}`,
        )
        .join(" ");
      logger.debug(
        `[patch:showSplitViewPanels] validTabs=${validTabs.length}/${tabs.length} input: ${inputDetail}`,
      );
      const validDetail = validTabs
        .map((t, i) => `[${i}]=${t.linkedPanel}`)
        .join(" ");
      logger.debug(
        `[patch:showSplitViewPanels] validTabs order: ${validDetail}`,
      );
      if (validTabs.length < 2) {
        logger.warn(
          "[patch:showSplitViewPanels] less than 2 valid tabs, skipping",
        );
        return;
      }

      // Ensure tabbar attributes are present whenever split view panels are
      // shown. This prevents theme CSS races where Lepton's not([multibar])
      // height clamps can re-apply and hide non-selected split tabs.
      const tabsToolbar = document?.getElementById("TabsToolbar");
      if (tabsToolbar) {
        if (!tabsToolbar.hasAttribute("multibar")) {
          tabsToolbar.setAttribute("multibar", "true");
          state.multibarSetBySplitView = true;
        }
        tabsToolbar.setAttribute("splitview-multibar", "true");
      }

      state.inShowSplitViewPanels = true;
      try {
        origShowSplitViewPanels!(validTabs);
        syncSplitTabMarkerAttrs();
        applySplitViewSessionMarkersForTabs(
          logger,
          validTabs,
          "showSplitViewPanels",
        );
        try {
          const ids = tabpanels.splitViewPanels;
          logger.debug(
            `[patch:showSplitViewPanels:afterNative] tabpanels.splitViewPanels=[${ids?.join(", ") ?? "undefined"}]`,
          );
        } catch (readErr) {
          logger.debug(
            `[patch:showSplitViewPanels:afterNative] could not read splitViewPanels: ${readErr}`,
          );
        }
        scheduleSplitPaneWarmRetries(logger);
        const bumpSplitViewUi = (): void => {
          ensureSplitPaneTabBrowsersAreWarmed(logger);
          ensureSplitPanelsActiveClassFromState();
          refreshActiveSplitPaneIndicator();
        };
        bumpSplitViewUi();
        requestAnimationFrame(() => {
          bumpSplitViewUi();
          requestAnimationFrame(() => bumpSplitViewUi());
        });
      } catch (e) {
        logger.error(`[patch:showSplitViewPanels] original threw: ${e}`);
      } finally {
        state.inShowSplitViewPanels = false;
      }
    };
    logger.debug("[patch] showSplitViewPanels patched");
  } else {
    logger.warn("[patch] showSplitViewPanels not found on gBrowser");
  }

  logger.debug("[patch] MozTabpanels patched successfully");

  return {
    unpatch() {
      const gBrowser = getGBrowser();
      if (!gBrowser?.tabpanels) return;

      const tabpanels = gBrowser.tabpanels;

      if (patchedSplitViewPanels) {
        delete (tabpanels as unknown as Record<string, unknown>)
          .splitViewPanels;
      }
      if (patchedIsSplitViewActive) {
        // Restore the original prototype method by removing the instance override
        delete (tabpanels as unknown as Record<string, unknown>)
          .setSplitViewActive;
      }
      if (origShowSplitViewPanels) {
        gBrowser.showSplitViewPanels = origShowSplitViewPanels;
      }

      tabpanels.removeAttribute("data-floorp-split");
      tabpanels.removeAttribute("split-view-layout");
      const tabsToolbar = document?.getElementById("TabsToolbar");
      if (tabsToolbar) {
        tabsToolbar.removeAttribute("splitview-multibar");
      }
      const splitMarkerTabs = document?.querySelectorAll<HTMLElement>(
        "#tabbrowser-tabs .tabbrowser-tab[data-floorp-split-tab]",
      );
      if (splitMarkerTabs) {
        for (const tab of splitMarkerTabs) {
          tab.removeAttribute("data-floorp-split-tab");
        }
      }
      logger.debug("[unpatch] MozTabpanels restored");
    },
  };
}
