// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  decorateGroup,
  getGroupKind,
  GROUP_KINDS_PREF,
  setGroupKind,
  TAB_EVENTS,
  updateGroupChips,
} from "../index.ts";
import {
  getActiveGroup,
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

function createTab(label: string, options: { hidden?: boolean } = {}): StackTab {
  const tab = createXULElement("tab") as unknown as StackTab;
  tab.id = `event-tab-${++tabCounter}`;
  setProperty(tab, "label", label);
  setProperty(tab, "selected", false);
  setProperty(tab, "linkedPanel", "");
  setProperty(tab, "hidden", options.hidden ?? false);
  setProperty(tab, "closing", false);
  setProperty(tab, "group", null);
  setProperty(tab, "isConnected", true);
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
  group.appendChild(container);
}

function createGroup(tabs: StackTab[], name: string): StackGroup {
  const group = createXULElement("tab-group") as unknown as StackGroup;
  group.id = `event-group-${++groupCounter}`;
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

/** The rebuild trigger list must cover every mutation that can change stack
 * contents or chip labels. Missing events mean stale mirrors. */
function testExactRebuildEventContract(): void {
  assertEquals(
    [...TAB_EVENTS].join(","),
    "TabSelect,TabClose,TabMove,TabAttrModified,TabGrouped,TabUngrouped,TabGroupCreate,TabGroupRemoved,TabGroupCollapse,TabGroupExpand",
    "TAB_EVENTS must exactly cover selection, membership, order, label and close changes",
  );
}

/** A rebuild must re-read the CURRENT native state, never cached data: after
 * a label/name change the chip must reflect it on the next decorate pass. */
function testDecorateReReadsLiveState(): void {
  const group = createGroup([createTab("A"), createTab("B")], "Before");
  decorateGroup(group, "stack", { tabGroups: [group] });

  setProperty(group, "label", "After");
  decorateGroup(group, "stack", { tabGroups: [group] });

  const label = group.querySelector<XULElement>(".tab-group-label");
  assertEquals(
    label?.getAttribute("data-floorp-title"),
    "After",
    "chip title comes from live group label, not a stale snapshot",
  );
}

/** Membership changes must flow into the chip count on the next pass. */
function testDecorateReReadsMembership(): void {
  const a = createTab("A");
  const b = createTab("B");
  const group = createGroup([a, b], "Stack");
  decorateGroup(group, "stack", { tabGroups: [group] });

  const c = createTab("C");
  setProperty(c, "group", group);
  const tabs = [...group.tabs, c];
  setProperty(group, "tabs", tabs);
  group.append(c);
  decorateGroup(group, "stack", { tabGroups: [group] });

  const label = group.querySelector<XULElement>(".tab-group-label");
  assertEquals(
    label?.getAttribute("data-floorp-count"),
    "3",
    "chip count re-enumerates live membership",
  );
}

/** Split-view groups must never be decorated as stacks regardless of events. */
function testSplitViewGroupsStayNative(): void {
  const splitTab = createTab("Split");
  splitTab.setAttribute("floorpSplitViewGroupId", "sv-1");
  const group = createGroup([splitTab], "Split");
  setProperty(group, "tabs", [splitTab]);
  setProperty(splitTab, "group", group);

  const gb = { tabGroups: [group] };
  updateGroupChips(gb);

  assertEquals(
    group.hasAttribute(STACK_ATTR),
    false,
    "split-view group is excluded from stack presentation",
  );
}

/** Workspaces hide groups with inline style.display — those must not surface
 * a bar even when they own the selected tab. */
function testWorkspaceHiddenGroupNeverActive(): void {
  const group = createGroup([createTab("A"), createTab("B")], "Stack");
  decorateGroup(group, "stack", { tabGroups: [group] });
  group.style.display = "none";

  const browser = makeBrowser([group], [...group.tabs], group.tabs[0]);
  withFakeBrowser(browser, () => {
    syncActiveGroup();
  });
  assertEquals(
    getActiveGroup(),
    null,
    "a workspace-hidden group never becomes active",
  );

  group.style.display = "";
  withFakeBrowser(browser, () => {
    syncActiveGroup();
  });
  assertEquals(
    getActiveGroup(),
    group,
    "unhiding the group restores its bar",
  );
}

/** Group kinds must survive restart: only explicit choices are stored and a
 * toggle back to stack removes the entry. */
function testKindChoicePersistsAndToggles(): void {
  const prefs = makePrefs();
  setGroupKind("g1", "group", prefs);
  assert(
    prefs.raw.has(GROUP_KINDS_PREF),
    "group kind is persisted to the pref",
  );
  assertEquals(getGroupKind("g1", prefs), "group", "choice survives a re-read");

  setGroupKind("g1", "stack", prefs);
  const stored = JSON.parse(prefs.raw.get(GROUP_KINDS_PREF) ?? "{}") as
    Record<string, string>;
  assertEquals(
    "g1" in stored,
    false,
    "default stack choice is not stored explicitly",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("tabStacksEventContract.test.ts", [
    {
      name: "exact rebuild event contract",
      fn: testExactRebuildEventContract,
    },
    {
      name: "decoration re-reads live group state",
      fn: testDecorateReReadsLiveState,
    },
    {
      name: "decoration re-reads live membership",
      fn: testDecorateReReadsMembership,
    },
    {
      name: "split-view groups stay native",
      fn: testSplitViewGroupsStayNative,
    },
    {
      name: "workspace-hidden groups never surface a bar",
      fn: testWorkspaceHiddenGroupNeverActive,
    },
    {
      name: "group kind choice persists and toggles",
      fn: testKindChoicePersistsAndToggles,
    },
  ]);
}
