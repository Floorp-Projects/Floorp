/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { SplitViewLayout } from "./data/types.js";
import { getGBrowser } from "./data/types.js";
import { updateHandles } from "./components/split-view-splitters.js";
import { getEffectiveSplitViewLayout } from "./utils/effective-layout.js";
import { resolveLayoutForSplitTabs } from "./patches/session-restore.js";

/**
 * Apply the current layout to the active split view.
 * Called reactively when the layout config signal changes.
 */
export function applyLayout(logger: ConsoleInstance): void {
  const gBrowser = getGBrowser();
  const activeSplitView = gBrowser?.activeSplitView;
  if (!activeSplitView) {
    logger.debug("[applyLayout] no activeSplitView, skipping");
    return;
  }

  const panels = gBrowser.tabpanels?.splitViewPanels;
  console.debug("[layout:applyLayout]", "called", {
    hasActiveSplitView: !!activeSplitView,
    panelsCount: panels?.length ?? "no panels",
  });
  if (!panels || panels.length < 2) {
    logger.debug(`[applyLayout] panels=${panels?.length ?? 0}, skipping`);
    return;
  }

  const layout = resolveLayoutForSplitTabs(activeSplitView.tabs);
  const tabpanels = document?.getElementById("tabbrowser-tabpanels");
  const currentAttr = tabpanels?.getAttribute("split-view-layout") ?? "(none)";
  console.debug("[layout:applyLayout]", "resolved layout", {
    resolvedLayout: layout,
    panelCount: panels.length,
    currentDomAttr: currentAttr,
  });
  logger.debug(`[applyLayout] layout=${layout}, panels=${panels.length}`);
  applyLayoutAttribute(logger, layout, panels.length);
  updateHandles(panels, layout);
}

/**
 * Set the split-view-layout attribute on the tabpanels element.
 */
export function applyLayoutAttribute(
  logger: ConsoleInstance,
  layout: SplitViewLayout,
  paneCount: number,
): void {
  const tabpanels = document?.getElementById("tabbrowser-tabpanels");
  if (!tabpanels) return;

  const effectiveLayout = getEffectiveSplitViewLayout(layout, paneCount);
  console.debug("[layout:applyLayoutAttribute]", "applying", {
    requestedLayout: layout,
    effectiveLayout,
    paneCount,
    willSetAttribute: effectiveLayout !== "horizontal",
  });
  if (effectiveLayout !== layout) {
    logger.debug(
      `[applyLayoutAttribute] layout=${layout} requires a different pane count, got ${paneCount}, falling back to horizontal`,
    );
  }

  if (effectiveLayout === "horizontal") {
    tabpanels.removeAttribute("split-view-layout");
    clearGridStyles(tabpanels);
  } else {
    tabpanels.setAttribute("split-view-layout", effectiveLayout);
  }
  logger.debug(
    `[applyLayoutAttribute] set effectiveLayout=${effectiveLayout}`,
  );
}

/**
 * Remove all grid-related inline styles from an element.
 */
export function clearGridStyles(el: Element): void {
  const style = (el as HTMLElement).style;
  style.removeProperty("grid-template-columns");
  style.removeProperty("grid-template-rows");
  style.removeProperty("--floorp-grid-col-ratio");
  style.removeProperty("--floorp-grid-row-ratio");
}
