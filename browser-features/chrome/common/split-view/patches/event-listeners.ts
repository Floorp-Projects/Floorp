/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getGBrowser } from "../data/types.js";
import { applyLayoutAttribute } from "../layout.js";
import { updateHandles } from "../components/split-view-splitters.js";
import { updatePaneDragGrips } from "../components/split-view-pane-drag.js";
import { getEffectiveSplitViewLayout } from "../utils/effective-layout.js";
import { resolveLayoutForSplitTabs } from "./session-restore.js";

/**
 * Listens for TabSplitViewActivate/Deactivate events and re-applies
 * layout when returning to a split view from a non-split tab.
 */
export function initSplitViewEvents(
  logger: ConsoleInstance,
): () => void {
  const tabContainer = getGBrowser()?.tabContainer;
  if (!tabContainer) return () => {};

  const onSplitViewActivate = (e: Event): void => {
    // Re-apply layout AFTER the entire event cascade has settled.
    // Only needed when returning to a split view from a non-split tab,
    // where the deactivation cleared the layout attributes.
    // Skip if layout was already applied by the splitViewPanels setter.
    const detail = (e as CustomEvent).detail;
    const tabs = detail?.tabs;
    if (!Array.isArray(tabs) || tabs.length < 2) return;

    requestAnimationFrame(() => {
      const gBrowser = getGBrowser();
      const tabpanels = document?.getElementById(
        "tabbrowser-tabpanels",
      );
      if (!tabpanels) return;

      const panels = gBrowser?.tabpanels?.splitViewPanels;
      if (!panels || panels.length < 2) return;

      const layout = resolveLayoutForSplitTabs(tabs);
      const currentLayoutAttr =
        tabpanels.getAttribute("split-view-layout") ?? "";
      const expectedLayoutResolved = getEffectiveSplitViewLayout(
        layout,
        panels.length,
      );
      const expectedLayout =
        expectedLayoutResolved === "horizontal" ? "" : expectedLayoutResolved;

      // Only re-apply if layout attribute doesn't match expected state
      if (currentLayoutAttr === expectedLayout) {
        const handleCount = tabpanels.querySelectorAll(
          ".floorp-split-handle, .floorp-grid-handle",
        ).length;
        if (handleCount > 0) {
          logger.debug(
            `[onSplitViewActivate:rAF] layout already correct (${expectedLayout || "horizontal"}), handles=${handleCount}, skipping`,
          );
          return;
        }
      }

      logger.debug(
        `[onSplitViewActivate:rAF] re-applying layout=${layout}, panels=${panels.length} (current="${currentLayoutAttr}", expected="${expectedLayout}")`,
      );
      applyLayoutAttribute(logger, layout, panels.length);
      updateHandles(panels, layout);
      updatePaneDragGrips(logger, panels);
    });
  };

  const onSplitViewDeactivate = (): void => {
    logger.debug("[onSplitViewDeactivate] clearing layout state");
    updatePaneDragGrips(logger, []);
  };

  tabContainer.addEventListener(
    "TabSplitViewActivate",
    onSplitViewActivate,
  );
  tabContainer.addEventListener(
    "TabSplitViewDeactivate",
    onSplitViewDeactivate,
  );

  logger.debug(
    "[events] TabSplitViewActivate/Deactivate listeners attached",
  );
  return () => {
    tabContainer.removeEventListener(
      "TabSplitViewActivate",
      onSplitViewActivate,
    );
    tabContainer.removeEventListener(
      "TabSplitViewDeactivate",
      onSplitViewDeactivate,
    );
  };
}
