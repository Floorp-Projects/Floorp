// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { canOpenContextTabsInSplitView } from "../utils/context-menu-policy.ts";
import type { SplitViewContextTab } from "../data/types.ts";
import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";

function tab(): SplitViewContextTab {
  return {
    pinned: false,
    splitview: null,
    hasAttribute: () => false,
    linkedBrowser: null,
    linkedPanel: "",
    selected: false,
    label: "Test tab",
  };
}

export async function runAllTests() {
  await runTests("split-view context menu", [
    {
      name: "Allows the partner picker and every supported pane count",
      fn: () => {
        for (const count of [1, 2, 3, 4]) {
          assertEquals(
            canOpenContextTabsInSplitView(
              Array.from({ length: count }, tab),
              4,
            ),
            true,
            `${count} tabs`,
          );
        }
      },
    },
    {
      name: "Rejects empty selections and excess tabs without truncating them",
      fn: () => {
        assertEquals(canOpenContextTabsInSplitView([], 4), false, "No tabs");
        for (const max of [2, 3, 4]) {
          const tabs = Array.from({ length: max + 1 }, tab);
          assertEquals(
            canOpenContextTabsInSplitView(tabs, max),
            false,
            `Limit ${max}`,
          );
          assertEquals(tabs.length, max + 1, "Selection was truncated");
        }
      },
    },
    {
      name: "Preserves native restrictions in mixed selections",
      fn: () => {
        const pinned = { ...tab(), pinned: true };
        const customizing = {
          ...tab(),
          hasAttribute: (name: string) => name === "customizemode",
        };
        const split = {
          ...tab(),
          splitview: document.createXULElement("hbox") as XULElement,
        };
        for (const blocked of [pinned, customizing, split]) {
          assertEquals(
            canOpenContextTabsInSplitView([tab(), blocked, tab()], 4),
            false,
            "Ineligible tab accepted",
          );
        }
      },
    },
    {
      name: "Checks the current limit again when configuration changes",
      fn: () => {
        const tabs = [tab(), tab(), tab()];
        assertEquals(
          canOpenContextTabsInSplitView(tabs, 4),
          true,
          "Initial limit",
        );
        assertEquals(
          canOpenContextTabsInSplitView(tabs, 2),
          false,
          "Reduced limit",
        );
      },
    },
  ]);
}
