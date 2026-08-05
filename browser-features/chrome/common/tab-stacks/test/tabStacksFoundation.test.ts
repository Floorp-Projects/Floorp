// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  decorateGroup,
  ENABLED_PREF,
  getGroupKind,
  GROUP_KINDS_PREF,
  isSplitViewGroup,
  readGroupKinds,
  setGroupKind,
  updateGroupChips,
} from "../index.ts";
import {
  activateGroup,
  findTabByDragId,
  getActiveGroup,
  getGroupDisplayTitle,
  getTabDragId,
  rememberSelection,
  type StackGroup,
  type StackTab,
  STACK_ATTR,
  syncActiveGroup,
  type TabBrowser,
} from "../stack-bar.tsx";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

type PrefsLike = {
  getStringPref(name: string, fallback?: string): string;
  setStringPref(name: string, value: string): void;
};

function setProperty(
  target: object,
  name: string,
  value: unknown,
): void {
  Object.defineProperty(target, name, {
    configurable: true,
    writable: true,
    value,
  });
}

function makePrefs(): PrefsLike & { raw: Map<string, string> } {
  const raw = new Map<string, string>();
  return {
    raw,
    getStringPref: (name: string, fallback?: string): string =>
      raw.get(name) ?? fallback ?? "",
    setStringPref: (name: string, value: string): void => {
      raw.set(name, value);
    },
  };
}

let tabCounter = 0;
let groupCounter = 0;

function createXULElement(tag: string): XULElement {
  return (document as unknown as {
    createXULElement: (tag: string) => XULElement;
  }).createXULElement(tag);
}

function createTab(
  label: string,
  options: { hidden?: boolean; split?: boolean } = {},
): StackTab {
  const tab = createXULElement("tab") as unknown as StackTab;
  tab.id = `test-tab-${++tabCounter}`;
  setProperty(tab, "label", label);
  setProperty(tab, "selected", false);
  setProperty(tab, "linkedPanel", "");
  setProperty(tab, "hidden", options.hidden ?? false);
  setProperty(tab, "closing", false);
  setProperty(tab, "group", null);
  // Detached elements report isConnected=false; the code uses it only as a
  // liveness guard, so pin it true for fixtures.
  setProperty(tab, "isConnected", true);
  if (options.split) {
    tab.setAttribute("floorpSplitViewGroupId", `sv-${tabCounter}`);
  }
  return tab;
}

function buildGroupChipDom(group: StackGroup, name: string): void {
  const container = createXULElement("hbox");
  container.classList.add("tab-group-label-container");
  const highlight = createXULElement("hbox");
  highlight.classList.add("tab-group-label-hover-highlight");
  const labelEl = createXULElement("label");
  labelEl.classList.add("tab-group-label");
  labelEl.textContent = name;
  highlight.appendChild(labelEl);
  container.appendChild(highlight);
  const countContainer = createXULElement("hbox");
  countContainer.classList.add("tab-group-overflow-count-container");
  const countEl = createXULElement("label");
  countEl.classList.add("tab-group-overflow-count");
  countContainer.appendChild(countEl);
  container.appendChild(countContainer);
  group.appendChild(container);
}

function createGroup(tabs: StackTab[], name: string): StackGroup {
  const group = createXULElement("tab-group") as unknown as StackGroup;
  group.id = `test-group-${++groupCounter}`;
  setProperty(group, "label", name);
  setProperty(group, "collapsed", false);
  setProperty(group, "tabs", tabs);
  for (const tab of tabs) {
    setProperty(tab, "group", group);
  }
  buildGroupChipDom(group, name);
  group.append(...tabs);
  return group;
}

function makeBrowser(
  groups: StackGroup[],
  tabs: StackTab[],
  selectedTab: StackTab,
): TabBrowser {
  const browser: TabBrowser = {
    tabs,
    selectedTab,
    tabGroups: groups,
    tabContainer: document.createDocumentFragment() as unknown as XULElement,
    removeTab: () => {},
    addTab: () => {
      throw new Error("addTab not implemented in fixture");
    },
  };
  return browser;
}

function withFakeBrowser<T>(
  browser: TabBrowser | null,
  fn: () => T,
): T {
  const prev = (globalThis as unknown as { gBrowser?: unknown }).gBrowser;
  (globalThis as unknown as { gBrowser?: TabBrowser | null }).gBrowser =
    browser;
  try {
    return fn();
  } finally {
    (globalThis as unknown as { gBrowser?: unknown }).gBrowser = prev;
  }
}

function testGroupKindPrefs(): void {
  const prefs = makePrefs();
  assertEquals(getGroupKind("g1", prefs), "stack", "unknown id defaults to stack");
  assertEquals(
    Object.keys(readGroupKinds(prefs)).length,
    0,
    "unset pref reads as empty map",
  );

  setGroupKind("g1", "group", prefs);
  assertEquals(getGroupKind("g1", prefs), "group", "explicit group choice persists");
  assertEquals(
    JSON.stringify(readGroupKinds(prefs)),
    '{"g1":"group"}',
    "group kind stored keyed by native group id",
  );

  setGroupKind("g2", "group", prefs);
  assertEquals(getGroupKind("g2", prefs), "group", "second group independent");

  setGroupKind("g1", "stack", prefs);
  assertEquals(
    getGroupKind("g1", prefs),
    "stack",
    "toggling back to stack removes the explicit entry",
  );
  assertEquals(
    JSON.stringify(readGroupKinds(prefs)),
    '{"g2":"group"}',
    "stale entry dropped when group is toggled back to a stack",
  );

  prefs.raw.set(GROUP_KINDS_PREF, "not-json");
  assertEquals(
    Object.keys(readGroupKinds(prefs)).length,
    0,
    "corrupt pref falls back to empty map",
  );
}

function testDecorateGroupStackKind(): void {
  const a = createTab("Alpha");
  const b = createTab("Beta");
  const group = createGroup([a, b], "Project tabs");
  const gb = { tabGroups: [group] };

  decorateGroup(group, "stack", gb);

  assertEquals(
    group.getAttribute(STACK_ATTR),
    "true",
    "stack group is marked with the ownership attribute",
  );
  const label = group.querySelector<XULElement>(".tab-group-label");
  assertEquals(
    label?.getAttribute("data-floorp-title"),
    "Project tabs",
    "chip title mirrors the group label",
  );
  assertEquals(
    label?.getAttribute("data-floorp-count"),
    "2",
    "chip count mirrors live membership",
  );
  assert(
    group.querySelector(".floorp-stack-icon"),
    "stack chip gets the stacked-layers glyph",
  );
  assert(
    group.querySelector(".floorp-stack-close"),
    "stack chip gets a close affordance",
  );
}

function testDecorateGroupAutoNamesUnnamed(): void {
  const first = createGroup([createTab("A")], "");
  const second = createGroup([createTab("B")], "");
  const gb = { tabGroups: [first, second] };

  decorateGroup(first, "stack", gb);
  decorateGroup(second, "stack", gb);

  assertEquals(first.label, "New Stack", "first unnamed group becomes New Stack");
  assertEquals(
    second.label,
    "New Stack 1",
    "second unnamed group increments the auto-name",
  );
  const label = first.querySelector<XULElement>(".tab-group-label");
  assertEquals(
    label?.getAttribute("data-floorp-title"),
    "New Stack",
    "chip title uses the stable auto-name",
  );
}

function testDecorateGroupForcesExpand(): void {
  const group = createGroup([createTab("A"), createTab("B")], "Stack");
  setProperty(group, "collapsed", true);
  decorateGroup(group, "stack", { tabGroups: [group] });
  assertEquals(group.collapsed, false, "stacks are always expanded");
}

function testDecorateGroupGroupKind(): void {
  const a = createTab("Alpha");
  const b = createTab("Beta");
  const group = createGroup([a, b], "Project tabs");
  const gb = { tabGroups: [group] };

  decorateGroup(group, "stack", gb);
  assert(group.hasAttribute(STACK_ATTR), "precondition: stacked");

  decorateGroup(group, "group", gb);

  assertEquals(
    group.hasAttribute(STACK_ATTR),
    false,
    "plain group loses the ownership attribute",
  );
  assertEquals(
    group.querySelector(".floorp-stack-icon"),
    null,
    "plain group loses the injected glyph",
  );
  assertEquals(
    group.querySelector(".floorp-stack-close"),
    null,
    "plain group loses the injected close button",
  );

  const unnamed = createGroup([createTab("C")], "");
  unnamed.setAttribute(STACK_ATTR, "true");
  decorateGroup(unnamed, "group", gb);
  assertEquals(
    unnamed.hasAttribute("data-floorp-unnamed"),
    true,
    "unnamed plain group is marked for the stylesheet",
  );
}

function testDecorateGroupGroupCountReachable(): void {
  const visible = createTab("A");
  const hidden = createTab("B", { hidden: true });
  const group = createGroup([visible, hidden], "Project");
  decorateGroup(group, "group", { tabGroups: [group] });
  const count = group.querySelector<XULElement>(".tab-group-overflow-count");
  assertEquals(
    count?.textContent,
    "1",
    "plain-group badge counts only workspace-reachable tabs",
  );
}

function testIsSplitViewGroup(): void {
  const normal = createGroup([createTab("A")], "Normal");
  assertEquals(
    isSplitViewGroup(normal),
    false,
    "ordinary group is not a split-view group",
  );

  const split = createGroup([createTab("B", { split: true })], "Split");
  assertEquals(
    isSplitViewGroup(split),
    true,
    "group owning a split-view tab is excluded",
  );
}

function testUpdateGroupChipsClassifies(): void {
  const normal = createGroup([createTab("A"), createTab("B")], "Stack");
  const split = createGroup([createTab("C", { split: true })], "Split");
  const gb = { tabGroups: [normal, split] };

  updateGroupChips(gb);

  assertEquals(
    normal.getAttribute(STACK_ATTR),
    "true",
    "ordinary group is presented as a stack",
  );
  assertEquals(
    split.hasAttribute(STACK_ATTR),
    false,
    "split-view group is left native",
  );
}

function testGetGroupDisplayTitle(): void {
  const hidden = createTab("Hidden long title", { hidden: true });
  const visible = createTab("Visible title");
  const group = createGroup([hidden, visible], "");
  assertEquals(
    getGroupDisplayTitle(group),
    "Visible title",
    "title uses the first reachable tab, never a workspace-hidden one",
  );

  const long = createGroup([createTab("x".repeat(100))], "");
  const title = getGroupDisplayTitle(long);
  assertEquals(title.length, 61, "title is bounded to 60 chars plus ellipsis");
  assertEquals(title.endsWith("…"), true, "bounded title ends with ellipsis");
}

function testActivateGroupPicksReachable(): void {
  const visible = createTab("A");
  const hidden = createTab("B", { hidden: true });
  const group = createGroup([visible, hidden], "Stack");
  decorateGroup(group, "stack", { tabGroups: [group] });
  const browser = makeBrowser([group], [visible, hidden], hidden);

  withFakeBrowser(browser, () => {
    activateGroup(group);
  });
  assertEquals(
    browser.selectedTab,
    visible,
    "activate skips workspace-hidden members and picks the first reachable",
  );

  // Remembered selection wins when it is still reachable.
  browser.selectedTab = visible;
  withFakeBrowser(browser, () => {
    rememberSelection();
    browser.selectedTab = hidden;
    activateGroup(group);
  });
  assertEquals(
    browser.selectedTab,
    visible,
    "remembered member is restored by chip activation",
  );
}

function testSyncActiveGroupOwnership(): void {
  const owned = createGroup([createTab("A"), createTab("B")], "Stack");
  decorateGroup(owned, "stack", { tabGroups: [owned] });
  const plain = createGroup([createTab("C"), createTab("D")], "Plain");
  const hiddenGroup = createGroup([createTab("E"), createTab("F")], "Hidden");
  hiddenGroup.style.display = "none";
  decorateGroup(hiddenGroup, "stack", { tabGroups: [hiddenGroup] });

  withFakeBrowser(null, () => {
    syncActiveGroup();
  });
  assertEquals(getActiveGroup(), null, "no browser yields no active group");

  const browser = makeBrowser(
    [owned, plain, hiddenGroup],
    [...owned.tabs, ...plain.tabs, ...hiddenGroup.tabs],
    owned.tabs[0],
  );
  withFakeBrowser(browser, () => {
    syncActiveGroup();
  });
  assertEquals(
    getActiveGroup(),
    owned,
    "owned group of the selected tab becomes active",
  );

  browser.selectedTab = plain.tabs[0];
  withFakeBrowser(browser, () => {
    syncActiveGroup();
  });
  assertEquals(
    getActiveGroup(),
    null,
    "a plain (non-owned) group never surfaces its bar",
  );

  browser.selectedTab = hiddenGroup.tabs[0];
  withFakeBrowser(browser, () => {
    syncActiveGroup();
  });
  assertEquals(
    getActiveGroup(),
    null,
    "a workspace-hidden group never surfaces its bar",
  );
}

function testDragIdRoundTrip(): void {
  const a = createTab("A");
  const b = createTab("B");
  const group = createGroup([a, b], "Stack");
  const browser = makeBrowser([group], [a, b], a);

  withFakeBrowser(browser, () => {
    const idA = getTabDragId(a);
    assertEquals(getTabDragId(a), idA, "drag id is stable per tab");
    assert(idA.length > 0, "drag id is non-empty");
    assertEquals(findTabByDragId(idA), a, "drag id resolves back to its tab");
    assertEquals(
      findTabByDragId("does-not-exist"),
      undefined,
      "unknown drag id resolves to nothing",
    );
  });
}

function testEnabledPrefConstant(): void {
  assertEquals(ENABLED_PREF, "floorp.tabstacks.enabled", "pref name constant");
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "group kind pref round-trips", fn: testGroupKindPrefs },
    { name: "stack decoration marks chip with title/count", fn: testDecorateGroupStackKind },
    { name: "unnamed stacks get stable auto-names", fn: testDecorateGroupAutoNamesUnnamed },
    { name: "stack decoration forces expanded groups", fn: testDecorateGroupForcesExpand },
    { name: "plain-group decoration restores native state", fn: testDecorateGroupGroupKind },
    { name: "plain-group badge counts reachable tabs", fn: testDecorateGroupGroupCountReachable },
    { name: "split-view groups are detected", fn: testIsSplitViewGroup },
    { name: "updateGroupChips classifies every group", fn: testUpdateGroupChipsClassifies },
    { name: "display title picks reachable anchor", fn: testGetGroupDisplayTitle },
    { name: "activate selects reachable members", fn: testActivateGroupPicksReachable },
    { name: "active group requires ownership and visibility", fn: testSyncActiveGroupOwnership },
    { name: "drag ids round-trip through gBrowser.tabs", fn: testDragIdRoundTrip },
    { name: "enabled pref constant is stable", fn: testEnabledPrefConstant },
  ];
  await runTests("tabStacksFoundation.test.ts", tests);
}
