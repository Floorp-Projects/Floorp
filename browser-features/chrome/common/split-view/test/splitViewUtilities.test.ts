// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { collectRestorableSplitGroups } from "../utils/collect-restorable-split-groups.ts";
import { getEffectiveSplitViewLayout } from "../utils/effective-layout.ts";
import {
  getGroupLayoutFromStore,
  getGroupPaneSizesFromStore,
  parseGroupLayoutStore,
  upsertGroupLayoutInStore,
  upsertGroupPaneSizesInStore,
} from "../utils/group-layout-store.ts";
import { hasLiveSplitPanelsState } from "../utils/live-split-ui.ts";
import { orderSplitGroupTabsForRestore } from "../utils/order-split-group-tabs.ts";
import { reorderSplitTabsForDesiredOrderImpl } from "../utils/reorder-strip-impl.ts";
import { resetSplitPanelPresentationState } from "../utils/reset-split-panel-presentation.ts";
import { runTests } from "../../../test/utils/test_harness.ts";

function assertEquals(
  actual: unknown,
  expected: unknown,
  message: string,
): void {
  if (!Object.is(actual, expected)) {
    throw new Error(
      `${message}: expected=${String(expected)} actual=${String(actual)}`,
    );
  }
}

function assertDeepEquals(
  actual: unknown,
  expected: unknown,
  message: string,
): void {
  const actualJson = JSON.stringify(actual);
  const expectedJson = JSON.stringify(expected);
  if (actualJson !== expectedJson) {
    throw new Error(
      `${message}: expected=${expectedJson} actual=${actualJson}`,
    );
  }
}

function testLayoutResolution(): void {
  assertEquals(
    getEffectiveSplitViewLayout("horizontal", 2),
    "horizontal",
    "horizontal layout",
  );
  assertEquals(
    getEffectiveSplitViewLayout("vertical", 3),
    "vertical",
    "vertical layout",
  );
  assertEquals(
    getEffectiveSplitViewLayout("grid-3pane-left-main", 3),
    "grid-3pane-left-main",
    "3-pane grid should remain for 3 panes",
  );
  assertEquals(
    getEffectiveSplitViewLayout("grid-3pane-left-main", 4),
    "horizontal",
    "3-pane grid should fallback for 4 panes",
  );
  assertEquals(
    getEffectiveSplitViewLayout("grid-2x2", 4),
    "grid-2x2",
    "2x2 should remain for 4 panes",
  );
  assertEquals(
    getEffectiveSplitViewLayout("grid-2x2", 3),
    "horizontal",
    "2x2 should fallback for 3 panes",
  );
}

function testLiveSplitState(): void {
  assertEquals(
    hasLiveSplitPanelsState(4, true),
    true,
    "4 panes + attr should be live",
  );
  assertEquals(
    hasLiveSplitPanelsState(2, true),
    true,
    "2 panes + attr should be live",
  );
  assertEquals(
    hasLiveSplitPanelsState(4, false),
    false,
    "missing attr should not be live",
  );
  assertEquals(
    hasLiveSplitPanelsState(1, true),
    false,
    "single pane should not be live",
  );
}

function testGroupStore(): void {
  const initial = parseGroupLayoutStore(
    JSON.stringify({
      groups: [{ groupId: "alpha", layout: "horizontal" }],
    }),
  );

  const updated = upsertGroupLayoutInStore(initial, "alpha", "grid-2x2");
  assertEquals(
    getGroupLayoutFromStore(updated, "alpha"),
    "grid-2x2",
    "layout upsert should update group",
  );

  const withPaneSizes = upsertGroupPaneSizesInStore(updated, "alpha", {
    flexRatios: [0.7, 0.3],
    gridColRatio: 0.6,
    gridRowRatio: 0.4,
  });
  assertDeepEquals(
    getGroupPaneSizesFromStore(withPaneSizes, "alpha"),
    {
      flexRatios: [0.7, 0.3],
      gridColRatio: 0.6,
      gridRowRatio: 0.4,
    },
    "pane size upsert should update matching group",
  );

  const invalid = parseGroupLayoutStore(
    JSON.stringify({
      groups: [
        { groupId: "invalid-layout", layout: "not-supported" },
        { groupId: 123, layout: "horizontal" },
        {
          groupId: "beta",
          layout: "vertical",
          paneSizes: {
            flexRatios: "bad",
            gridColRatio: 0.5,
            gridRowRatio: 0.5,
          },
        },
      ],
    }),
  );
  assertDeepEquals(
    invalid,
    {
      groups: [{ groupId: "beta", layout: "vertical" }],
    },
    "invalid entries should be filtered while keeping valid layout rows",
  );
}

function testCollectRestorableSplitGroups(): void {
  const strip = [
    { id: "a1", groupId: "alpha", order: 1 },
    { id: "x", groupId: null, order: 99 },
    { id: "b1", groupId: "beta", order: 0 },
    { id: "a2", groupId: "alpha", order: 0 },
    { id: "b2", groupId: "beta", order: 1 },
    { id: "solo", groupId: "solo", order: 0 },
  ];

  const groups = collectRestorableSplitGroups(
    strip,
    4,
    (tab) => tab.groupId,
    (tabs) => [...tabs].sort((a, b) => a.order - b.order),
  );

  assertDeepEquals(
    groups,
    [
      {
        groupId: "alpha",
        tabs: [
          { id: "a2", groupId: "alpha", order: 0 },
          { id: "a1", groupId: "alpha", order: 1 },
        ],
      },
      {
        groupId: "beta",
        tabs: [
          { id: "b1", groupId: "beta", order: 0 },
          { id: "b2", groupId: "beta", order: 1 },
        ],
      },
    ],
    "restore groups should preserve first-seen group order and drop solo groups",
  );

  const truncated = collectRestorableSplitGroups(
    [
      { id: "g1", groupId: "gamma", order: 0 },
      { id: "g2", groupId: "gamma", order: 1 },
      { id: "g3", groupId: "gamma", order: 2 },
    ],
    2,
    (tab) => tab.groupId,
    (tabs) => tabs,
  );
  assertEquals(truncated.length, 1, "single group should remain restorable");
  assertEquals(
    truncated[0]?.tabs.length,
    2,
    "group tab list should respect maxPanes cap",
  );
}

function testOrderSplitGroupTabsForRestore(): void {
  const strip = ["a", "b", "c", "d"] as const;
  const group = ["c", "a", "d", "b"];
  const idx = new Map<string, number>([
    ["a", 0],
    ["b", 1],
    ["c", 2],
    ["d", 3],
  ]);

  const withPaneIndex = orderSplitGroupTabsForRestore(group, strip, (tab) =>
    idx.get(tab),
  );
  assertDeepEquals(
    withPaneIndex,
    ["a", "b", "c", "d"],
    "pane index should dominate restore ordering",
  );

  const stripFallback = orderSplitGroupTabsForRestore(
    ["b", "a", "z"],
    ["x", "a", "y", "b", "z"],
    (tab) => (tab === "z" ? Number.NaN : undefined),
  );
  assertDeepEquals(
    stripFallback,
    ["a", "b", "z"],
    "missing or non-finite pane indexes should fall back to strip order",
  );
}

function moveTabBeforeInArray(
  tabs: string[],
  tab: string,
  before: string | null,
): void {
  const from = tabs.indexOf(tab);
  if (from < 0) {
    return;
  }
  tabs.splice(from, 1);
  if (before === null) {
    tabs.push(tab);
    return;
  }
  const to = tabs.indexOf(before);
  if (to < 0) {
    tabs.push(tab);
    return;
  }
  tabs.splice(to, 0, tab);
}

function testReorderSplitTabsForDesiredOrderImpl(): void {
  const cases: { strip: string[]; desired: string[]; want: string[] }[] = [
    {
      strip: ["A", "B", "C", "D"],
      desired: ["B", "C", "A", "D"],
      want: ["B", "C", "A", "D"],
    },
    {
      strip: ["X", "Y", "A", "B", "C", "D", "Z"],
      desired: ["C", "A", "B", "D"],
      want: ["X", "Y", "C", "A", "B", "D", "Z"],
    },
    {
      strip: ["A", "B", "C", "D"],
      desired: ["D", "C", "B", "A"],
      want: ["D", "C", "B", "A"],
    },
  ];

  for (const { strip, desired, want } of cases) {
    const tabs = [...strip];
    reorderSplitTabsForDesiredOrderImpl(
      () => tabs,
      (tab, before) => moveTabBeforeInArray(tabs, tab, before),
      desired,
    );
    assertDeepEquals(
      tabs,
      want,
      "strip reorder should keep block start and desired relative order",
    );
  }

  const unchanged = ["A", "B", "C"];
  reorderSplitTabsForDesiredOrderImpl(
    () => unchanged,
    (tab, before) => moveTabBeforeInArray(unchanged, tab, before),
    ["A", "MISSING"],
  );
  assertDeepEquals(
    unchanged,
    ["A", "B", "C"],
    "reorder should no-op when desired tabs are missing",
  );
}

class FakeClassList {
  constructor(private readonly names: Set<string>) {}

  contains(name: string): boolean {
    return this.names.has(name);
  }

  remove(...names: string[]): void {
    for (const name of names) {
      this.names.delete(name);
    }
  }
}

class FakePanel {
  readonly classNames: Set<string>;
  readonly attrs: Set<string>;
  readonly classList: FakeClassList;

  constructor(classNames: string[], attrs: string[]) {
    this.classNames = new Set(classNames);
    this.attrs = new Set(attrs);
    this.classList = new FakeClassList(this.classNames);
  }

  hasAttribute(name: string): boolean {
    return this.attrs.has(name);
  }

  removeAttribute(name: string): void {
    this.attrs.delete(name);
  }
}

function testResetSplitPanelPresentationState(): void {
  const panel = new FakePanel(
    ["split-view-panel", "split-view-panel-active", "deck-selected"],
    [
      "column",
      "data-floorp-active-pane",
      "data-floorp-drag-source",
      "data-floorp-drop-target",
    ],
  );
  assertEquals(
    resetSplitPanelPresentationState(panel),
    true,
    "panel with split-view state should be reset",
  );
  assertEquals(
    panel.classList.contains("split-view-panel"),
    false,
    "split-view-panel class should be removed",
  );
  assertEquals(
    panel.classList.contains("split-view-panel-active"),
    false,
    "active class should be removed",
  );
  assertEquals(
    panel.classList.contains("deck-selected"),
    false,
    "deck-selected class should be removed",
  );
  assertEquals(
    panel.hasAttribute("column"),
    false,
    "column attribute should be removed",
  );
  assertEquals(
    panel.hasAttribute("data-floorp-active-pane"),
    false,
    "active pane marker should be removed",
  );

  const noResetPanel = new FakePanel(["deck-selected"], []);
  assertEquals(
    resetSplitPanelPresentationState(noResetPanel),
    false,
    "panel without split markers should not reset",
  );
  assertEquals(
    noResetPanel.classList.contains("deck-selected"),
    true,
    "unrelated class should remain untouched",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("splitViewUtilities.test.ts", [
    { name: "layout resolution", fn: testLayoutResolution },
    { name: "live split state", fn: testLiveSplitState },
    { name: "group store", fn: testGroupStore },
    {
      name: "collect restorable split groups",
      fn: testCollectRestorableSplitGroups,
    },
    {
      name: "order split group tabs for restore",
      fn: testOrderSplitGroupTabsForRestore,
    },
    {
      name: "reorder split tabs for desired order",
      fn: testReorderSplitTabsForDesiredOrderImpl,
    },
    {
      name: "reset split panel presentation state",
      fn: testResetSplitPanelPresentationState,
    },
  ]);
}
