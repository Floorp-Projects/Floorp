// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  type NativeStackGroup,
  type NativeStackTab,
  TAB_STACKS_REBUILD_EVENTS,
  TAB_STACKS_ROW_ID,
  TAB_STACKS_STYLE_ID,
  type TabStacksBrowser,
  TabStacksController,
  type TabStacksServices,
  VERTICAL_TABS_PREF,
  WORKSPACES_CHANGED_TOPIC,
} from "../stack-bar.tsx";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

type Observer = (
  subject?: unknown,
  topic?: string,
  data?: string,
) => void;

class TrackingEventTarget extends EventTarget {
  readonly added: string[] = [];
  readonly removed: string[] = [];

  override addEventListener(
    type: string,
    callback: EventListener | null,
    options?: boolean | AddEventListenerOptions,
  ): void {
    this.added.push(type);
    super.addEventListener(type, callback, options);
  }

  override removeEventListener(
    type: string,
    callback: EventListener | null,
    options?: boolean | EventListenerOptions,
  ): void {
    this.removed.push(type);
    super.removeEventListener(type, callback, options);
  }
}

class FakeServices implements TabStacksServices {
  vertical = false;
  readonly prefObservers: Observer[] = [];
  readonly removedPrefObservers: Observer[] = [];
  readonly workspaceObservers: Observer[] = [];
  readonly removedWorkspaceObservers: Observer[] = [];

  readonly prefs = {
    getBoolPref: (name: string, fallback?: boolean): boolean =>
      name === VERTICAL_TABS_PREF ? this.vertical : fallback ?? false,
    addObserver: (name: string, observer: Observer): void => {
      assertEquals(
        name,
        VERTICAL_TABS_PREF,
        "only vertical pref may be observed",
      );
      this.prefObservers.push(observer);
    },
    removeObserver: (name: string, observer: Observer): void => {
      assertEquals(
        name,
        VERTICAL_TABS_PREF,
        "only vertical pref may be removed",
      );
      this.removedPrefObservers.push(observer);
    },
  };

  readonly obs = {
    addObserver: (observer: Observer, topic: string): void => {
      assertEquals(
        topic,
        WORKSPACES_CHANGED_TOPIC,
        "workspace topic must be exact",
      );
      this.workspaceObservers.push(observer);
    },
    removeObserver: (observer: Observer, topic: string): void => {
      assertEquals(
        topic,
        WORKSPACES_CHANGED_TOPIC,
        "workspace removal must be exact",
      );
      this.removedWorkspaceObservers.push(observer);
    },
  };

  notifyWorkspace(): void {
    for (const observer of this.workspaceObservers) {
      if (!this.removedWorkspaceObservers.includes(observer)) {
        observer(null, WORKSPACES_CHANGED_TOPIC);
      }
    }
  }
}

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

function row(document: Document): HTMLElement | null {
  return document.getElementById(TAB_STACKS_ROW_ID) as HTMLElement | null;
}

function count(document: Document): string | null | undefined {
  return row(document)?.querySelector(".floorp-tab-stacks-group-count")
    ?.textContent;
}

function dispatch(target: EventTarget, type: string, detail?: unknown): void {
  target.dispatchEvent(new CustomEvent(type, { detail }));
}

function testExactEventsAlwaysRebuildFromLiveState(): void {
  const testDocument = document.implementation.createHTMLDocument(
    "tab-stacks-events",
  );
  const toolbox = testDocument.createElement("div");
  toolbox.id = "navigator-toolbox";
  const navBar = testDocument.createElement("div");
  navBar.id = "nav-bar";
  toolbox.appendChild(navBar);
  testDocument.body!.appendChild(toolbox);

  const makeTab = (label: string, id: string): NativeStackTab => {
    const tab = testDocument.createElement("tab") as unknown as NativeStackTab;
    tab.id = id;
    setProperty(tab, "label", label);
    setProperty(tab, "linkedPanel", "");
    setProperty(tab, "splitview", null);
    setProperty(tab, "closing", false);
    return tab;
  };
  const first = makeTab("First", "event-tab-1");
  const second = makeTab("Second", "event-tab-2");
  const third = makeTab("Third", "event-tab-3");
  const liveTabs = [first, second];

  const group = testDocument.createElement(
    "tab-group",
  ) as unknown as NativeStackGroup;
  group.id = "event-group";
  setProperty(group, "label", "Before update");
  setProperty(group, "defaultGroupName", "Default group");
  setProperty(group, "color", "blue");
  setProperty(group, "collapsed", false);
  Object.defineProperty(group, "tabs", {
    configurable: true,
    get: () => liveTabs,
  });
  Object.defineProperty(group, "tabsAndSplitViews", {
    configurable: true,
    get: () => [...liveTabs],
  });
  group.style.setProperty("--tab-group-color", "var(--tab-group-color-blue)");
  group.append(first, second);
  testDocument.body!.appendChild(group);

  const browser: TabStacksBrowser = {
    tabGroups: [group],
    visibleTabs: [first, second],
    selectedTab: first,
  };
  const events = new TrackingEventTarget();
  const services = new FakeServices();
  const controller = new TabStacksController({
    document: testDocument,
    eventTarget: events,
    services,
    getBrowser: () => browser,
  });
  controller.init();

  assertEquals(
    events.added.filter((type) => type !== "unload").join(","),
    TAB_STACKS_REBUILD_EVENTS.join(","),
    "controller should attach exactly the locked Firefox 153 rebuild events",
  );
  assert(row(testDocument), "initial live group should render");

  const beforeUpdate = row(testDocument);
  setProperty(group, "label", "After update");
  setProperty(group, "color", "purple");
  group.style.setProperty("--tab-group-color", "var(--tab-group-color-purple)");
  dispatch(events, "TabGroupUpdate", { staleLabel: "ignored" });
  assert(
    row(testDocument) !== beforeUpdate,
    "TabGroupUpdate should rebuild row DOM",
  );
  assertEquals(
    row(testDocument)?.getAttribute("aria-label"),
    "After update",
    "TabGroupUpdate should read the current native label, not event detail",
  );
  assertEquals(
    row(testDocument)?.dataset.groupColor,
    "purple",
    "TabGroupUpdate should read the current native color",
  );

  liveTabs.push(third);
  group.appendChild(third);
  browser.visibleTabs = [first, second, third];
  dispatch(events, "TabGrouped", first);
  assertEquals(
    count(testDocument),
    "3",
    "TabGrouped should enumerate live membership instead of event.detail",
  );

  liveTabs.pop();
  third.remove();
  browser.visibleTabs = [first, second];
  dispatch(events, "TabUngrouped", third);
  assertEquals(
    count(testDocument),
    "2",
    "TabUngrouped should enumerate live membership after the change",
  );

  browser.visibleTabs = [first];
  dispatch(events, "TabHide");
  assertEquals(row(testDocument), null, "TabHide should apply live visibility");
  browser.visibleTabs = [first, second];
  dispatch(events, "TabShow");
  assert(row(testDocument), "TabShow should rebuild eligible live members");

  browser.selectedTab = second;
  dispatch(events, "TabSelect");
  const selected = row(testDocument)?.querySelectorAll<HTMLElement>(
    ".floorp-tab-stacks-proxy",
  )[1];
  assertEquals(
    selected?.getAttribute("aria-selected"),
    "true",
    "TabSelect should mirror current native selection",
  );

  setProperty(group, "collapsed", true);
  dispatch(events, "TabGroupCollapse");
  assertEquals(row(testDocument), null, "collapse should remove the mirror");
  setProperty(group, "collapsed", false);
  dispatch(events, "TabGroupExpand");
  assert(row(testDocument), "expand should regenerate the mirror");

  browser.tabGroups = [];
  dispatch(events, "TabGroupRemoved");
  assertEquals(
    row(testDocument),
    null,
    "removed live group should remove the row",
  );
  browser.tabGroups = [group];
  dispatch(events, "TabGroupCreate");
  assert(row(testDocument), "created live group should regenerate the row");

  browser.visibleTabs = [second];
  services.notifyWorkspace();
  assertEquals(
    row(testDocument),
    null,
    "workspace notification should filter off-workspace members from live state",
  );
  browser.visibleTabs = [first, second];
  services.notifyWorkspace();
  assert(
    row(testDocument),
    "workspace notification should restore current members",
  );

  services.vertical = true;
  services.prefObservers[0](null, VERTICAL_TABS_PREF);
  assertEquals(row(testDocument), null, "vertical pref should destroy the row");
  assertEquals(
    testDocument.getElementById(TAB_STACKS_STYLE_ID),
    null,
    "vertical pref should destroy owned styles",
  );
  services.vertical = false;
  services.prefObservers[0](null, VERTICAL_TABS_PREF);
  assert(row(testDocument), "horizontal pref should rebuild from native state");

  dispatch(events, "unload");
  assertEquals(row(testDocument), null, "unload should remove the row");
  assertEquals(
    testDocument.getElementById(TAB_STACKS_STYLE_ID),
    null,
    "unload should remove styles",
  );
  assertEquals(
    events.removed.filter((type) => type !== "unload").join(","),
    TAB_STACKS_REBUILD_EVENTS.join(","),
    "unload should remove every exact native event listener",
  );
  assertEquals(
    events.removed.filter((type) => type === "unload").length,
    1,
    "unload should remove its own window listener",
  );
  assertEquals(
    services.removedPrefObservers[0],
    services.prefObservers[0],
    "unload should remove the exact pref observer identity",
  );
  assertEquals(
    services.removedWorkspaceObservers[0],
    services.workspaceObservers[0],
    "unload should remove the exact workspace observer identity",
  );

  controller.destroy();
  assertEquals(
    services.removedPrefObservers.length,
    1,
    "destroy after unload should remain idempotent",
  );
  assertEquals(
    events.removed.filter((type) => type === "unload").length,
    1,
    "destroy after unload must not repeat listener teardown",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("tabStacksEventContract.test.ts", [
    {
      name: "exact Firefox events rebuild from live native state and clean up",
      fn: testExactEventsAlwaysRebuildFromLiveState,
    },
  ]);
}
