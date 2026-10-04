/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { SplitViewWrapper } from "../data/types.js";
import { getGBrowser } from "../data/types.js";
import { reorderSplitTabsForDesiredOrder } from "../utils/reorder-panes.js";

export interface WrapperPatchResult {
  unpatch(): void;
}

/**
 * Patches MozTabSplitViewWrapper.reverseTabs to support N-pane (>2) reversal.
 * The upstream implementation only handles 2-pane reversal.
 */
export function patchSplitViewWrapper(
  logger: ConsoleInstance,
): WrapperPatchResult | null {
  const WrapperClass = customElements.get("tab-split-view-wrapper");
  if (!WrapperClass) {
    logger.warn(
      "[patch] tab-split-view-wrapper not registered, skipping",
    );
    return null;
  }

  const wrapperProto = WrapperClass.prototype as
    & SplitViewWrapper
    & Record<string, unknown>;
  const origReverseTabs = wrapperProto.reverseTabs;

  Object.defineProperty(wrapperProto, "reverseTabs", {
    value: function (this: SplitViewWrapper) {
      const tabs = this.tabs;
      logger.debug(`[patch:reverseTabs] tabs.length=${tabs.length}`);
      if (tabs.length === 2 && origReverseTabs) {
        origReverseTabs.call(this);
        return;
      }

      const gBrowser = getGBrowser();
      if (!gBrowser) return;
      const reversed = [...tabs].reverse();
      reorderSplitTabsForDesiredOrder(gBrowser, reversed);
      gBrowser.showSplitViewPanels(reversed);
    },
    configurable: true,
    writable: true,
  });

  logger.debug("[patch] MozTabSplitViewWrapper.reverseTabs patched");

  return {
    unpatch() {
      Object.defineProperty(wrapperProto, "reverseTabs", {
        value: origReverseTabs,
        configurable: true,
        writable: true,
      });
      logger.debug(
        "[unpatch] MozTabSplitViewWrapper.reverseTabs restored",
      );
    },
  };
}
