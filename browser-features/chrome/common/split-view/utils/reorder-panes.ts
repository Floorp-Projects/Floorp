/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  getGBrowser,
  type SplitViewGBrowser,
  type SplitViewTab,
} from "../data/types.js";
import { reorderSplitTabsForDesiredOrderImpl } from "./reorder-strip-impl.js";

export { reorderSplitTabsForDesiredOrderImpl };

/**
 * Reorder tabs in the tab strip so the given split tabs appear left-to-right
 * as `desiredOrder`, while keeping the block anchored at the same strip index
 * (contiguous split group).
 */
export function reorderSplitTabsForDesiredOrder(
  gBrowser: SplitViewGBrowser,
  desiredOrder: SplitViewTab[],
): void {
  reorderSplitTabsForDesiredOrderImpl<SplitViewTab>(
    () => gBrowser.tabs,
    (tab, before) => gBrowser.moveTabBefore(tab, before),
    desiredOrder,
  );
}

function describeTabLine(tab: SplitViewTab, idx: number): string {
  const spec = (tab.linkedBrowser as { currentURI?: { spec?: string } } | null)
    ?.currentURI?.spec ?? "";
  const lab = (tab.label || "").slice(0, 36);
  return `[${idx}] linkedPanel=${tab.linkedPanel} label="${lab}" spec=${
    spec.slice(0, 56)
  }`;
}

function logTabsSnapshot(
  logger: ConsoleInstance,
  phase: string,
  arr: SplitViewTab[],
): void {
  const lines = arr.map((t, i) => describeTabLine(t, i));
  logger.debug(
    `[swapPanesByTab:${phase}] count=${arr.length}\n${lines.join("\n")}`,
  );
}

/** Visual tab strip order: linkedPanel ids for tabs currently in split view. */
function linkedPanelsInStripOrder(gBrowser: SplitViewGBrowser): string[] {
  const out: string[] = [];
  for (const t of gBrowser.tabs) {
    const st = t as SplitViewTab;
    if (st.splitview) {
      out.push(st.linkedPanel);
    }
  }
  return out;
}

/**
 * Swap two panes by tab reference, resolving indices at execution time
 * to avoid stale closures. Reorders the tab strip to match the new pane order,
 * then calls showSplitViewPanels with that order.
 */
export function swapPanesByTab(
  logger: ConsoleInstance,
  fromTab: SplitViewTab,
  toTab: SplitViewTab,
): void {
  const gBrowser = getGBrowser();
  const activeSplitView = gBrowser?.activeSplitView;
  if (!activeSplitView || !gBrowser) {
    return;
  }

  const tabs = activeSplitView.tabs;
  const fromIndex = tabs.indexOf(fromTab);
  const toIndex = tabs.indexOf(toTab);

  if (fromIndex === -1 || toIndex === -1) {
    logger.warn(
      `[swapPanesByTab] tab(s) no longer in split view: from=${fromIndex}, to=${toIndex}`,
    );
    return;
  }

  logger.debug(
    `[swapPanesByTab] swapping pane ${fromIndex} <-> ${toIndex}`,
  );

  logTabsSnapshot(logger, "wrapper.tabs.before", tabs);

  const reordered = [...tabs];
  [reordered[fromIndex], reordered[toIndex]] = [
    reordered[toIndex]!,
    reordered[fromIndex]!,
  ];

  logTabsSnapshot(logger, "reordered.intended", reordered);
  logger.debug(
    `[swapPanesByTab] intended linkedPanels left-to-right=[${
      reordered.map((t) => t.linkedPanel).join(", ")
    }]`,
  );

  const stripBefore = linkedPanelsInStripOrder(gBrowser);
  logger.debug(
    `[swapPanesByTab] tabStrip.splitTabs.linkedPanels.before=[${
      stripBefore.join(", ")
    }]`,
  );

  reorderSplitTabsForDesiredOrder(gBrowser, reordered);

  const stripAfter = linkedPanelsInStripOrder(gBrowser);
  logger.debug(
    `[swapPanesByTab] tabStrip.splitTabs.linkedPanels.afterMove=[${
      stripAfter.join(", ")
    }]`,
  );
  const expectedPanels = reordered.map((t) => t.linkedPanel);
  if (stripAfter.join(",") !== expectedPanels.join(",")) {
    logger.warn(
      `[swapPanesByTab] tab strip order !== reordered linkedPanels: ` +
        `strip=[${stripAfter.join(", ")}] expected=[${
          expectedPanels.join(", ")
        }]`,
    );
  }

  // Must pass the intended left-to-right order. `activeSplitView.tabs` can
  // still reflect the pre-swap wrapper order until the UI catches up.
  logger.debug(
    `[swapPanesByTab] calling showSplitViewPanels(reordered) with ${reordered.length} tab(s)`,
  );
  gBrowser.showSplitViewPanels(reordered);

  logger.debug(
    `[swapPanesByTab] after showSplitViewPanels wrapper.tabs=[${
      activeSplitView.tabs.map((t) => t.linkedPanel).join(", ")
    }]`,
  );
}
