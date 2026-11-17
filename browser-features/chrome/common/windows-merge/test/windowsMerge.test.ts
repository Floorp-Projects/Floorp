// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import enUS from "#i18n/en-US/browser-chrome.json" with {
  type: "json",
};
import jaJP from "#i18n/ja-JP/browser-chrome.json" with {
  type: "json",
};
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { gestureActions } from "../../mouse-gesture/utils/gestures.ts";
import {
  getPaletteCommands,
  invalidateCache,
} from "../../command-palette/command-registry.ts";
import {
  adoptItem,
  classifyStripElement,
  isMergeableWindow,
  MERGE_WINDOWS_ACTION_ID,
  mergeAllWindows,
  type MergeTabBrowser,
  type MergeWindow,
  type StripItem,
  stripItems,
} from "../controller.ts";
import { mergeWindowsIntoOne } from "../merge.ts";
import type { MergeableItem, MergeResult, MergeSource } from "../types.ts";

// ---------------------------------------------------------------------------
// Fake windows for the merge core
// ---------------------------------------------------------------------------

interface FakeItem extends MergeableItem {
  readonly name: string;
  /** Tab names this item carries, in strip order. */
  readonly tabs: string[];
}

interface FakeSource extends MergeSource {
  readonly name: string;
  readonly items: FakeItem[];
}

interface FakeTarget {
  tabs: string[];
  pinnedCount: number;
}

interface FakeRun {
  readonly result: MergeResult;
  readonly target: FakeTarget;
  readonly closed: string[];
}

interface FakeRunOptions {
  readonly sources: FakeSource[];
  targetTabs?: string[];
  targetPinned?: number;
  targetIsPrivate?: boolean;
  failItemNames?: string[];
  keepWindowsOpen?: string[];
}

function tabItem(name: string, pinned = false): FakeItem {
  return { kind: "tab", pinned, tabCount: 1, name, tabs: [name] };
}

function groupItem(name: string, tabs: string[]): FakeItem {
  return { kind: "group", pinned: false, tabCount: tabs.length, name, tabs };
}

function source(
  name: string,
  items: FakeItem[],
  isPrivate = false,
): FakeSource {
  return { name, items, isPrivate };
}

function runMerge(options: FakeRunOptions): FakeRun {
  const target: FakeTarget = {
    tabs: [...(options.targetTabs ?? [])],
    pinnedCount: options.targetPinned ?? 0,
  };
  const closed: string[] = [];
  const failed = new Set(options.failItemNames ?? []);
  const keepOpen = new Set(options.keepWindowsOpen ?? []);

  const result = mergeWindowsIntoOne(options.sources, {
    targetIsPrivate: options.targetIsPrivate ?? false,
    itemsOf: (itemSource) => itemSource.items,
    nextIndex: (pinned) => pinned ? target.pinnedCount : target.tabs.length,
    adopt: (item, index) => {
      if (failed.has(item.name)) return false;
      target.tabs.splice(index, 0, ...item.tabs);
      if (item.pinned) target.pinnedCount += item.tabs.length;
      return true;
    },
    closeSource: (itemSource) => {
      if (keepOpen.has(itemSource.name)) return false;
      closed.push(itemSource.name);
      return true;
    },
  });

  return { result, target, closed };
}

// ---------------------------------------------------------------------------
// Merge core
// ---------------------------------------------------------------------------

function testMergesEveryTabInWindowOrder(): void {
  const run = runMerge({
    targetTabs: ["t1", "t2"],
    sources: [
      source("A", [tabItem("a1"), tabItem("a2")]),
      source("B", [tabItem("b1")]),
    ],
  });

  assertEquals(
    run.target.tabs.join(","),
    "t1,t2,a1,a2,b1",
    "source tabs should be appended in window order",
  );
  assertEquals(run.result.movedItems, 3, "three items moved");
  assertEquals(run.result.movedTabs, 3, "three tabs moved");
  assertEquals(run.result.failedItems, 0, "no failures");
  assertEquals(run.result.closedWindows, 2, "both emptied windows closed");
  assertEquals(run.closed.join(","), "A,B", "windows closed in merge order");
}

function testPinnedTabsLandAfterTargetPinnedTabs(): void {
  const run = runMerge({
    targetTabs: ["p1", "t1"],
    targetPinned: 1,
    sources: [source("A", [tabItem("a1", true), tabItem("a2")])],
  });

  assertEquals(
    run.target.tabs.join(","),
    "p1,a1,t1,a2",
    "pinned source tab joins the pinned area, unpinned one is appended",
  );
  assertEquals(run.target.pinnedCount, 2, "pinned area grows by one");
}

function testGroupsMoveAsOneItem(): void {
  const run = runMerge({
    targetTabs: ["t1"],
    sources: [source("A", [groupItem("group", ["g1", "g2", "g3"])])],
  });

  assertEquals(
    run.target.tabs.join(","),
    "t1,g1,g2,g3",
    "group members move together and keep their order",
  );
  assertEquals(run.result.movedItems, 1, "a group counts as one item");
  assertEquals(run.result.movedTabs, 3, "all group tabs are counted");
}

function testPrivateWindowsAreSkippedWithNormalTarget(): void {
  const run = runMerge({
    targetTabs: ["t1"],
    sources: [
      source("private", [tabItem("p1")], true),
      source("normal", [tabItem("n1")]),
    ],
  });

  assertEquals(
    run.target.tabs.join(","),
    "t1,n1",
    "private tabs must not enter a normal window",
  );
  assertEquals(run.result.skippedWindows, 1, "one window skipped");
  assertEquals(run.closed.join(","), "normal", "only the merged window closed");
}

function testPrivateTargetOnlyTakesPrivateWindows(): void {
  const run = runMerge({
    targetTabs: ["t1"],
    targetIsPrivate: true,
    sources: [
      source("normal", [tabItem("n1")]),
      source("private", [tabItem("p1")], true),
    ],
  });

  assertEquals(
    run.target.tabs.join(","),
    "t1,p1",
    "a private window only takes private tabs",
  );
  assertEquals(run.result.skippedWindows, 1, "one window skipped");
}

function testFailedAdoptionKeepsWindowOpen(): void {
  const run = runMerge({
    targetTabs: ["t1"],
    sources: [source("A", [tabItem("a1"), tabItem("a2")])],
    failItemNames: ["a2"],
  });

  assertEquals(
    run.target.tabs.join(","),
    "t1,a1",
    "the movable tab still moves",
  );
  assertEquals(run.result.failedItems, 1, "the refused item is counted");
  assertEquals(
    run.result.closedWindows,
    0,
    "window with leftover tabs stays open",
  );
  assertEquals(run.closed.length, 0, "nothing was closed");
}

function testWindowWithTabsIsNotCountedAsClosed(): void {
  const run = runMerge({
    targetTabs: ["t1"],
    sources: [source("A", [tabItem("a1")])],
    keepWindowsOpen: ["A"],
  });

  assertEquals(run.result.movedItems, 1, "the tab moved");
  assertEquals(run.result.closedWindows, 0, "window still owned tabs");
}

function testEmptySourceIsSkipped(): void {
  const run = runMerge({
    targetTabs: ["t1"],
    sources: [source("A", [])],
  });

  assertEquals(run.result.skippedWindows, 1, "empty window skipped");
  assertEquals(run.result.movedItems, 0, "nothing moved");
  assertEquals(run.closed.length, 0, "empty window left alone");
}

// ---------------------------------------------------------------------------
// Tab strip classification
// ---------------------------------------------------------------------------

function makeElement(localName: string): Element {
  return document.createElement(localName);
}

function testClassifiesStripElements(): void {
  assertEquals(classifyStripElement(makeElement("tab")), "tab", "tab");
  assertEquals(
    classifyStripElement(makeElement("tab-group")),
    "group",
    "group",
  );
  assertEquals(
    classifyStripElement(makeElement("tab-split-view-wrapper")),
    "splitView",
    "split view wrapper",
  );
  assertEquals(
    classifyStripElement(makeElement("arrowscrollbox-periphery")),
    null,
    "periphery elements are not mergeable",
  );
}

interface FakeStrip {
  readonly browser: MergeTabBrowser;
}

function makeStrip(
  pinned: Array<{ tag: string; closing?: boolean; tabs?: number }>,
  unpinned: Array<{ tag: string; closing?: boolean; tabs?: number }>,
): FakeStrip {
  const build = (
    container: Element,
    specs: Array<{ tag: string; closing?: boolean; tabs?: number }>,
  ) => {
    for (const spec of specs) {
      const element = document.createElement(spec.tag) as Element & {
        closing?: boolean;
        tabs?: unknown[];
      };
      if (spec.closing) element.closing = true;
      if (typeof spec.tabs === "number") {
        element.tabs = Array.from(
          { length: spec.tabs },
          () => document.createElement("tab"),
        );
      }
      container.append(element);
    }
  };

  const pinnedTabsContainer = document.createElement("div");
  const arrowScrollbox = document.createElement("div");
  build(pinnedTabsContainer, pinned);
  build(arrowScrollbox, unpinned);

  const tabs: XULElement[] = [];
  const browser: MergeTabBrowser = {
    tabs,
    pinnedTabCount: pinned.length,
    tabContainer: { pinnedTabsContainer, arrowScrollbox },
    adoptTab: () => null,
  };
  return { browser };
}

function testStripItemsKeepOrderAndSkipPeriphery(): void {
  const { browser } = makeStrip(
    [{ tag: "tab" }, { tag: "arrowscrollbox-periphery" }],
    [
      { tag: "tab" },
      { tag: "tab-group", tabs: 3 },
      { tag: "tab-split-view-wrapper", tabs: 2 },
      { tag: "arrowscrollbox-periphery" },
      { tag: "tab", closing: true },
    ],
  );

  const items = stripItems(browser);
  assertEquals(
    items.map((item) => `${item.kind}:${item.pinned}:${item.tabCount}`).join(
      "|",
    ),
    "tab:true:1|tab:false:1|group:false:3|splitView:false:2",
    "strip items keep order, pin state and tab counts",
  );
}

// ---------------------------------------------------------------------------
// Fails closed without a browser window
// ---------------------------------------------------------------------------

function testMergeWithoutBrowserWindowDoesNothing(): void {
  const result = mergeAllWindows({} as unknown as Window);
  assertEquals(result.movedItems, 0, "no items moved");
  assertEquals(result.closedWindows, 0, "no windows closed");
  assertEquals(result.skippedWindows, 0, "nothing counted");
}

// ---------------------------------------------------------------------------
// Action catalog
// ---------------------------------------------------------------------------

function testActionIsRegisteredInTheStaticCatalog(): void {
  const registered = gestureActions.getAction(MERGE_WINDOWS_ACTION_ID);
  assert(
    typeof registered === "function",
    "merge action should be part of the static gesture catalog",
  );
}

function testActionIdIsStable(): void {
  assertEquals(
    MERGE_WINDOWS_ACTION_ID,
    "floorp-merge-all-windows",
    "action id is part of the user-facing shortcut config",
  );
}

function testPaletteSurfacesTheAction(): void {
  invalidateCache();
  try {
    const entry = getPaletteCommands(window as unknown as Window).find(
      (command) => command.id === MERGE_WINDOWS_ACTION_ID,
    );

    assert(entry, "the command palette should list the merge action");
    assertEquals(entry.category, "window", "palette category");
    assert(entry.label.length > 0, "palette label is set");
    assert(
      entry.label !== MERGE_WINDOWS_ACTION_ID,
      "palette label comes from the chrome locales",
    );
  } finally {
    invalidateCache();
  }
}

// ---------------------------------------------------------------------------
// Member state of adopted groups and split views
// ---------------------------------------------------------------------------

interface FakeAdoptTarget extends MergeTabBrowser {
  readonly hidden: XULElement[];
}

function makeTabElement(workspaceId?: string, hidden = false): XULElement {
  const tab = document.createElement("tab") as unknown as XULElement;
  if (workspaceId) tab.setAttribute("floorpWorkspaceId", workspaceId);
  if (hidden) tab.setAttribute("hidden", "true");
  return tab;
}

function makeContainer(
  tag: string,
  members: XULElement[],
): XULElement & { tabs: XULElement[] } {
  const container = document.createElement(tag) as unknown as XULElement & {
    tabs: XULElement[];
  };
  container.tabs = members;
  return container;
}

function makeTarget(adoptedContainer: XULElement): FakeAdoptTarget {
  const hidden: XULElement[] = [];
  return {
    tabs: [],
    pinnedTabCount: 0,
    hidden,
    adoptTab: () => null,
    adoptTabGroup: () => adoptedContainer as XULElement,
    adoptSplitView: () => adoptedContainer as XULElement,
    hideTab: (tab: XULElement) => {
      hidden.push(tab);
    },
  };
}

function stripItemFor(
  kind: StripItem["kind"],
  element: XULElement,
  tabCount: number,
): StripItem {
  return { kind, pinned: false, tabCount, element };
}

function testAdoptedGroupKeepsMemberState(): void {
  const sourceGroup = makeContainer("tab-group", [
    makeTabElement("workspace-a"),
    makeTabElement("workspace-b", true),
  ]);
  const adoptedMembers = [makeTabElement(), makeTabElement()];
  const adoptedGroup = makeContainer("tab-group", adoptedMembers);
  const target = makeTarget(adoptedGroup);

  const moved = adoptItem(
    stripItemFor("group", sourceGroup as unknown as XULElement, 2),
    0,
    target,
  );

  assert(moved, "the group should be adopted");
  assertEquals(
    adoptedMembers[0].getAttribute("floorpWorkspaceId"),
    "workspace-a",
    "first member keeps its workspace",
  );
  assertEquals(
    adoptedMembers[1].getAttribute("floorpWorkspaceId"),
    "workspace-b",
    "second member keeps its workspace",
  );
  assertEquals(
    target.hidden.length,
    1,
    "only the hidden source member is hidden again",
  );
  assert(
    target.hidden[0] === adoptedMembers[1],
    "the hidden member that gets re-hidden is the mapped one",
  );
}

function testAdoptedSplitViewKeepsMemberState(): void {
  const sourceSplitView = makeContainer("tab-split-view-wrapper", [
    makeTabElement(undefined, true),
    makeTabElement("workspace-c"),
  ]);
  const adoptedMembers = [makeTabElement(), makeTabElement()];
  const adoptedSplitView = makeContainer(
    "tab-split-view-wrapper",
    adoptedMembers,
  );
  const target = makeTarget(adoptedSplitView);

  const moved = adoptItem(
    stripItemFor("splitView", sourceSplitView as unknown as XULElement, 2),
    0,
    target,
  );

  assert(moved, "the split view should be adopted");
  assertEquals(
    target.hidden.length,
    1,
    "the hidden pane is hidden again",
  );
  assertEquals(
    adoptedMembers[1].getAttribute("floorpWorkspaceId"),
    "workspace-c",
    "visible member keeps its workspace",
  );
}

function testAdoptedContainerLengthMismatchIsSafe(): void {
  const sourceGroup = makeContainer("tab-group", [
    makeTabElement("workspace-a"),
    makeTabElement("workspace-b"),
  ]);
  const adoptedGroup = makeContainer("tab-group", [makeTabElement()]);
  const target = makeTarget(adoptedGroup);

  assert(
    adoptItem(
      stripItemFor("group", sourceGroup as unknown as XULElement, 2),
      0,
      target,
    ),
    "the group should be adopted",
  );
  assertEquals(
    adoptedGroup.tabs[0].getAttribute("floorpWorkspaceId"),
    "workspace-a",
    "the members that exist are mapped, the extra one is ignored",
  );
}

// ---------------------------------------------------------------------------
// Popup windows are not merge sources
// ---------------------------------------------------------------------------

function fakeWindow(options: {
  isTarget?: boolean;
  closed?: boolean;
  hasBrowser?: boolean;
  toolbarVisible?: boolean | undefined;
}): MergeWindow {
  return {
    closed: options.closed ?? false,
    gBrowser: options.hasBrowser === false
      ? undefined
      : ({} as MergeTabBrowser),
    toolbar: options.toolbarVisible === undefined
      ? undefined
      : { visible: options.toolbarVisible },
  } as unknown as MergeWindow;
}

function testPopupWindowsAreNotMergeable(): void {
  const target = {} as Window;
  const sameWindow = fakeWindow({ toolbarVisible: true });

  assert(
    isMergeableWindow(fakeWindow({ toolbarVisible: true }), target),
    "a normal browser window is mergeable",
  );
  assert(
    isMergeableWindow(fakeWindow({ toolbarVisible: undefined }), target),
    "a window without a toolbar object is still mergeable",
  );
  assertEquals(
    isMergeableWindow(fakeWindow({ toolbarVisible: false }), target),
    false,
    "a popup without a toolbar is not merged",
  );
  assertEquals(
    isMergeableWindow(sameWindow, sameWindow as unknown as Window),
    false,
    "the target window is never a source",
  );
  assertEquals(
    isMergeableWindow(
      fakeWindow({ hasBrowser: false, toolbarVisible: true }),
      target,
    ),
    false,
    "a window without gBrowser is not merged",
  );
  assertEquals(
    isMergeableWindow(fakeWindow({ closed: true }), target),
    false,
    "a closed window is not merged",
  );
}

// ---------------------------------------------------------------------------
// Localized labels
// ---------------------------------------------------------------------------

function testChromeLocales(): void {
  const en = enUS.mouseGesture;
  const ja = jaJP.mouseGesture;
  const enActions = en.actions as Record<string, string | undefined>;
  const enDescriptions = en.descriptions as Record<string, string | undefined>;
  const jaActions = ja.actions as Record<string, string | undefined>;
  const jaDescriptions = ja.descriptions as Record<string, string | undefined>;

  assert(
    typeof enActions[MERGE_WINDOWS_ACTION_ID] === "string" &&
      enActions[MERGE_WINDOWS_ACTION_ID] !== MERGE_WINDOWS_ACTION_ID,
    "English label should exist",
  );
  assert(
    typeof enDescriptions[MERGE_WINDOWS_ACTION_ID] === "string",
    "English description should exist",
  );
  assert(
    typeof jaActions[MERGE_WINDOWS_ACTION_ID] === "string" &&
      jaActions[MERGE_WINDOWS_ACTION_ID] !== MERGE_WINDOWS_ACTION_ID,
    "Japanese label should exist",
  );
  assert(
    typeof jaDescriptions[MERGE_WINDOWS_ACTION_ID] === "string",
    "Japanese description should exist",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "moves every tab in window order",
      fn: testMergesEveryTabInWindowOrder,
    },
    {
      name: "pinned tabs land after target pinned tabs",
      fn: testPinnedTabsLandAfterTargetPinnedTabs,
    },
    { name: "groups move as one item", fn: testGroupsMoveAsOneItem },
    {
      name: "private windows are skipped for a normal target",
      fn: testPrivateWindowsAreSkippedWithNormalTarget,
    },
    {
      name: "private target only takes private windows",
      fn: testPrivateTargetOnlyTakesPrivateWindows,
    },
    {
      name: "failed adoption keeps the window open",
      fn: testFailedAdoptionKeepsWindowOpen,
    },
    {
      name: "windows with leftover tabs are not counted as closed",
      fn: testWindowWithTabsIsNotCountedAsClosed,
    },
    { name: "empty source is skipped", fn: testEmptySourceIsSkipped },
    {
      name: "classifies tab strip elements",
      fn: testClassifiesStripElements,
    },
    {
      name: "strip items keep order and skip periphery",
      fn: testStripItemsKeepOrderAndSkipPeriphery,
    },
    {
      name: "merge without a browser window does nothing",
      fn: testMergeWithoutBrowserWindowDoesNothing,
    },
    {
      name: "action is part of the static catalog",
      fn: testActionIsRegisteredInTheStaticCatalog,
    },
    { name: "action id is stable", fn: testActionIdIsStable },
    {
      name: "command palette surfaces the action",
      fn: testPaletteSurfacesTheAction,
    },
    {
      name: "adopted groups keep member workspace and hidden state",
      fn: testAdoptedGroupKeepsMemberState,
    },
    {
      name: "adopted split views keep member workspace and hidden state",
      fn: testAdoptedSplitViewKeepsMemberState,
    },
    {
      name: "adopted container length mismatch is safe",
      fn: testAdoptedContainerLengthMismatchIsSafe,
    },
    {
      name: "popup windows are not merge sources",
      fn: testPopupWindowsAreNotMergeable,
    },
    { name: "EN/JA chrome locales", fn: testChromeLocales },
  ];
  await runTests("windowsMerge.test.ts", tests);
}
