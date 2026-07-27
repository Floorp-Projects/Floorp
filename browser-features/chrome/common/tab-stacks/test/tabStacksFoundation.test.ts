// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  initializeTabStacksFoundation,
  TAB_STACKS_ENABLED_PREF,
} from "../index.ts";
import {
  groupContainsSplitView,
  type NativeStackGroup,
  type NativeStackTab,
  TAB_STACKS_REBUILD_EVENTS,
  TAB_STACKS_ROW_ID,
  TAB_STACKS_STYLE_ID,
  type TabStacksBrowser,
  type TabStacksController,
  type TabStacksServices,
  VERTICAL_TABS_PREF,
  WORKSPACES_CHANGED_TOPIC,
} from "../stack-bar.tsx";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
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
  readonly values = new Map<string, boolean>();
  readonly getCalls: Array<{ name: string; fallback: boolean | undefined }> =
    [];
  readonly prefAdded: Array<{ name: string; observer: Observer }> = [];
  readonly prefRemoved: Array<{ name: string; observer: Observer }> = [];
  readonly topicAdded: Array<{ topic: string; observer: Observer }> = [];
  readonly topicRemoved: Array<{ topic: string; observer: Observer }> = [];
  persistenceWrites = 0;

  readonly prefs = {
    getBoolPref: (name: string, fallback?: boolean): boolean => {
      this.getCalls.push({ name, fallback });
      return this.values.get(name) ?? fallback ?? false;
    },
    addObserver: (name: string, observer: Observer): void => {
      this.prefAdded.push({ name, observer });
    },
    removeObserver: (name: string, observer: Observer): void => {
      this.prefRemoved.push({ name, observer });
    },
    setBoolPref: (): void => {
      this.persistenceWrites++;
    },
    setStringPref: (): void => {
      this.persistenceWrites++;
    },
  };

  readonly obs = {
    addObserver: (observer: Observer, topic: string): void => {
      this.topicAdded.push({ topic, observer });
    },
    removeObserver: (observer: Observer, topic: string): void => {
      this.topicRemoved.push({ topic, observer });
    },
  };

  notifyPref(name: string): void {
    for (const entry of this.prefAdded) {
      if (
        entry.name === name &&
        !this.prefRemoved.some((removed) =>
          removed.name === name && removed.observer === entry.observer
        )
      ) {
        entry.observer(null, name);
      }
    }
  }
}

interface Harness {
  document: Document;
  events: TrackingEventTarget;
  services: FakeServices;
  tabs: NativeStackTab[];
  group: NativeStackGroup;
  browser: TabStacksBrowser;
  controller: TabStacksController;
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

function createTestDocument(): Document {
  const testDocument = document.implementation.createHTMLDocument(
    "tab-stacks-test",
  );
  const toolbox = testDocument.createElement("div");
  toolbox.id = "navigator-toolbox";
  const tabsToolbar = testDocument.createElement("div");
  tabsToolbar.id = "TabsToolbar";
  const navBar = testDocument.createElement("div");
  navBar.id = "nav-bar";
  toolbox.append(tabsToolbar, navBar);
  testDocument.body!.appendChild(toolbox);
  return testDocument;
}

function createTab(
  testDocument: Document,
  label: string,
  id: string,
): NativeStackTab {
  const tab = testDocument.createElement("tab") as unknown as NativeStackTab;
  tab.id = id;
  setProperty(tab, "label", label);
  setProperty(tab, "linkedPanel", `${id}-panel`);
  setProperty(tab, "closing", false);
  setProperty(tab, "splitview", null);
  return tab;
}

function createHarness(
  options: {
    labels?: string[];
    groupLabel?: string;
    defaultGroupName?: string;
    color?: string;
    selectedIndex?: number;
  } = {},
): Harness {
  const testDocument = createTestDocument();
  const labels = options.labels ?? ["Alpha", "Beta", "Gamma"];
  const tabs = labels.map((label, index) =>
    createTab(testDocument, label, `native-tab-${index}`)
  );

  const group = testDocument.createElement(
    "tab-group",
  ) as unknown as NativeStackGroup;
  group.id = "native-group-1";
  setProperty(group, "label", options.groupLabel ?? "Project tabs");
  setProperty(group, "defaultGroupName", options.defaultGroupName ?? "Group");
  setProperty(group, "color", options.color ?? "blue");
  setProperty(group, "collapsed", false);
  setProperty(group, "tabs", tabs);
  setProperty(group, "tabsAndSplitViews", [...tabs]);
  group.style.setProperty("--tab-group-color", "var(--tab-group-color-blue)");
  group.append(...tabs);
  testDocument.body!.appendChild(group);

  const selectedIndex = options.selectedIndex ?? 0;
  const browser: TabStacksBrowser = {
    tabGroups: [group],
    visibleTabs: [...tabs],
    selectedTab: tabs[selectedIndex],
  };
  const events = new TrackingEventTarget();
  const services = new FakeServices();
  services.values.set(TAB_STACKS_ENABLED_PREF, true);
  services.values.set(VERTICAL_TABS_PREF, false);

  const controller = initializeTabStacksFoundation({
    document: testDocument,
    eventTarget: events,
    services,
    getBrowser: () => browser,
  });
  assert(controller, "enabled fixture should create a controller");

  return {
    document: testDocument,
    events,
    services,
    tabs,
    group,
    browser,
    controller,
  };
}

function proxies(testDocument: Document): HTMLElement[] {
  return Array.from(
    testDocument.querySelectorAll<HTMLElement>(
      `#${TAB_STACKS_ROW_ID} .floorp-tab-stacks-proxy`,
    ),
  );
}

function syntheticKeydown(key: string): Event {
  const event = new Event("keydown", {
    bubbles: true,
    cancelable: true,
  });
  setProperty(event, "key", key);
  return event;
}

function testMissingPrefIsInert(): void {
  const testDocument = createTestDocument();
  const events = new TrackingEventTarget();
  const services = new FakeServices();

  const controller = initializeTabStacksFoundation({
    document: testDocument,
    eventTarget: events,
    services,
    getBrowser: () => null,
  });

  assertEquals(controller, null, "missing enabled pref should stay disabled");
  assertEquals(services.getCalls.length, 1, "enabled pref should be read once");
  assertEquals(
    services.getCalls[0].name,
    TAB_STACKS_ENABLED_PREF,
    "startup should read the approved feature pref",
  );
  assertEquals(
    services.getCalls[0].fallback,
    false,
    "missing feature pref must use a false fallback",
  );
  assertEquals(
    events.added.length,
    0,
    "pref-off startup must add no listeners",
  );
  assertEquals(
    services.prefAdded.length,
    0,
    "pref-off startup must add no pref observers",
  );
  assertEquals(
    services.topicAdded.length,
    0,
    "pref-off startup must add no Services observers",
  );
  assertEquals(
    testDocument.getElementById(TAB_STACKS_ROW_ID),
    null,
    "pref-off startup must not mount a row",
  );
  assertEquals(
    testDocument.getElementById(TAB_STACKS_STYLE_ID),
    null,
    "pref-off startup must not mount styles",
  );
}

function testPresentationAndAccessibility(): void {
  const harness = createHarness();
  try {
    const { document: testDocument, group, tabs } = harness;
    const nativeBefore = group.outerHTML;
    const panel = testDocument.createElement("div");
    panel.id = "native-tab-0-panel";
    testDocument.body!.appendChild(panel);
    harness.controller.rebuild();

    const row = testDocument.getElementById(TAB_STACKS_ROW_ID);
    assert(row, "eligible active native group should render a row");
    assertEquals(
      row.getAttribute("role"),
      "tablist",
      "row role should be tablist",
    );
    assertEquals(
      row.getAttribute("aria-label"),
      "Project tabs",
      "row accessible name should equal the native group name",
    );
    assertEquals(
      (row as HTMLElement).dataset.groupColor,
      "blue",
      "row should reflect the native group color code",
    );
    assertEquals(
      (row as HTMLElement).style.getPropertyValue("--floorp-tab-stacks-color"),
      "var(--tab-group-color-blue)",
      "row should reflect the native group color value",
    );
    assertEquals(
      row.querySelector(".floorp-tab-stacks-group-count")?.textContent,
      "3",
      "count should use the eligible live member count",
    );
    assertEquals(
      row.querySelector(".floorp-tab-stacks-group-meta")?.getAttribute(
        "aria-hidden",
      ),
      "true",
      "visual group metadata and count should be presentational",
    );

    const rendered = proxies(testDocument);
    assertEquals(
      rendered.length,
      3,
      "all eligible native members should proxy",
    );
    rendered.forEach((proxy, index) => {
      assertEquals(
        proxy.getAttribute("role"),
        "tab",
        "proxy role should be tab",
      );
      assertEquals(
        proxy.getAttribute("aria-label"),
        tabs[index].label ?? "",
        "proxy accessible name should equal the native tab label",
      );
      assertEquals(
        proxy.getAttribute("aria-selected"),
        String(index === 0),
        "proxy selected state should mirror the native selected tab",
      );
      assertEquals(
        proxy.tabIndex,
        index === 0 ? 0 : -1,
        "proxy tabindex should rove from the selected native tab",
      );
    });
    assertEquals(
      rendered[0].getAttribute("aria-controls"),
      "native-tab-0-panel",
      "proxy should expose aria-controls only for an existing linked panel",
    );
    assertEquals(
      rendered[1].hasAttribute("aria-controls"),
      false,
      "proxy should omit aria-controls when its panel is absent",
    );
    assertEquals(
      group.outerHTML,
      nativeBefore,
      "rendering must not mutate native group or member presentation",
    );
  } finally {
    harness.controller.destroy();
  }
}

function testEligibilityFallbackAndSplitExclusion(): void {
  const harness = createHarness({
    groupLabel: "",
    defaultGroupName: "Unnamed group",
  });
  try {
    const { browser, controller, document: testDocument, group, tabs } =
      harness;
    tabs[1].hidden = true;
    browser.visibleTabs = [tabs[0], tabs[1], tabs[2]];
    controller.rebuild();

    const row = testDocument.getElementById(TAB_STACKS_ROW_ID);
    assert(row, "two eligible visible members should still render");
    assertEquals(
      row.getAttribute("aria-label"),
      "Unnamed group",
      "empty native label should use defaultGroupName exactly",
    );
    assertEquals(
      proxies(testDocument).length,
      2,
      "hidden members should be filtered",
    );
    assertEquals(
      proxies(testDocument)[1].getAttribute("aria-label"),
      "Gamma",
      "off-workspace and hidden members must not become proxy targets",
    );

    setProperty(group, "collapsed", true);
    controller.rebuild();
    assertEquals(
      testDocument.getElementById(TAB_STACKS_ROW_ID),
      null,
      "collapsed active group should render no mirror",
    );
    assertEquals(
      group.collapsed,
      true,
      "mirror must not expand a native group",
    );

    setProperty(group, "collapsed", false);
    setProperty(tabs[2], "splitview", testDocument.createElement("div"));
    controller.rebuild();
    assertEquals(
      testDocument.getElementById(TAB_STACKS_ROW_ID),
      null,
      "any split member should exclude the whole native group",
    );
    assert(
      groupContainsSplitView(group),
      "split membership should be detected",
    );

    setProperty(tabs[2], "splitview", null);
    const wrapper = testDocument.createElement("tab-split-view-wrapper");
    setProperty(group, "tabsAndSplitViews", [tabs[0], wrapper]);
    controller.rebuild();
    assertEquals(
      testDocument.getElementById(TAB_STACKS_ROW_ID),
      null,
      "a split-view wrapper should exclude the whole native group",
    );

    setProperty(group, "tabsAndSplitViews", [...tabs]);
    setProperty(group, "tabs", []);
    controller.rebuild();
    assertEquals(
      testDocument.getElementById(TAB_STACKS_ROW_ID),
      null,
      "an empty live group should render no mirror",
    );

    setProperty(group, "tabs", tabs);
    browser.visibleTabs = [tabs[0]];
    controller.rebuild();
    assertEquals(
      testDocument.getElementById(TAB_STACKS_ROW_ID),
      null,
      "one eligible member should render no mirror",
    );
    assertEquals(
      harness.services.persistenceWrites,
      0,
      "foundation must not write custom persistence",
    );
  } finally {
    harness.controller.destroy();
  }
}

function testClickAndKeyboardSelection(): void {
  const harness = createHarness();
  try {
    const { browser, controller, document: testDocument, events, tabs } =
      harness;
    for (const tab of tabs) {
      tab.id = "";
    }
    controller.rebuild();
    let rendered = proxies(testDocument);
    let focused = -1;
    rendered.forEach((proxy, index) => {
      proxy.focus = () => {
        focused = index;
      };
    });

    rendered[0].dispatchEvent(syntheticKeydown("ArrowRight"));
    assertEquals(focused, 1, "ArrowRight should move focus to the next proxy");
    assertEquals(
      browser.selectedTab,
      tabs[0],
      "arrow navigation should not change native selection",
    );
    rendered[0].dispatchEvent(syntheticKeydown("ArrowLeft"));
    assertEquals(focused, 2, "ArrowLeft should wrap focus to the final proxy");

    rendered[1].dispatchEvent(syntheticKeydown("End"));
    assertEquals(focused, 2, "End should move focus to the final proxy");
    rendered[2].dispatchEvent(syntheticKeydown("Home"));
    assertEquals(focused, 0, "Home should move focus to the first proxy");

    rendered[1].dispatchEvent(syntheticKeydown("Enter"));
    assertEquals(
      browser.selectedTab,
      tabs[1],
      "Enter should select the corresponding existing native tab",
    );
    events.dispatchEvent(new Event("TabSelect"));
    rendered = proxies(testDocument);
    assertEquals(
      rendered[1].getAttribute("aria-selected"),
      "true",
      "TabSelect rebuild should mirror the new native selection",
    );
    assertEquals(
      rendered[1].tabIndex,
      0,
      "newly selected proxy should own tabindex 0",
    );

    rendered[2].dispatchEvent(syntheticKeydown(" "));
    assertEquals(
      browser.selectedTab,
      tabs[2],
      "Space should select the corresponding existing native tab",
    );

    rendered[0].dispatchEvent(new Event("click", { bubbles: true }));
    assertEquals(
      browser.selectedTab,
      tabs[0],
      "click should select the corresponding existing native tab",
    );

    tabs[1].hidden = true;
    browser.visibleTabs = [tabs[0], tabs[2]];
    rendered[1].dispatchEvent(new Event("click", { bubbles: true }));
    assertEquals(
      browser.selectedTab,
      tabs[0],
      "a stale proxy must not retarget to a different live native tab",
    );
    assertEquals(
      proxies(testDocument).map((proxy) => proxy.getAttribute("aria-label"))
        .join(","),
      "Alpha,Gamma",
      "stale activation should rebuild to the eligible live member list",
    );
  } finally {
    harness.controller.destroy();
  }
}

function testVerticalLifecycleAndExplicitDestroy(): void {
  const harness = createHarness();
  const nativeBefore = harness.group.outerHTML;
  assert(
    harness.document.getElementById(TAB_STACKS_ROW_ID),
    "horizontal enabled state should start with a row",
  );

  harness.services.values.set(VERTICAL_TABS_PREF, true);
  harness.services.notifyPref(VERTICAL_TABS_PREF);
  assertEquals(
    harness.document.getElementById(TAB_STACKS_ROW_ID),
    null,
    "vertical tabs should destroy the mirror row",
  );
  assertEquals(
    harness.document.getElementById(TAB_STACKS_STYLE_ID),
    null,
    "vertical tabs should destroy owned styles",
  );
  assertEquals(
    harness.group.outerHTML,
    nativeBefore,
    "vertical transition must leave native presentation untouched",
  );

  harness.services.values.set(VERTICAL_TABS_PREF, false);
  harness.services.notifyPref(VERTICAL_TABS_PREF);
  assert(
    harness.document.getElementById(TAB_STACKS_ROW_ID),
    "returning horizontal should rebuild from live native state",
  );

  harness.controller.destroy();
  assertEquals(
    harness.document.getElementById(TAB_STACKS_ROW_ID),
    null,
    "explicit destroy should remove the row",
  );
  assertEquals(
    harness.document.getElementById(TAB_STACKS_STYLE_ID),
    null,
    "explicit destroy should remove styles",
  );
  assertEquals(
    harness.services.prefRemoved.length,
    1,
    "explicit destroy should remove the vertical pref observer",
  );
  assertEquals(
    harness.services.prefRemoved[0].name,
    VERTICAL_TABS_PREF,
    "only the vertical lifecycle pref should be observed",
  );
  assertEquals(
    harness.services.topicRemoved[0].topic,
    WORKSPACES_CHANGED_TOPIC,
    "explicit destroy should remove the workspace observer",
  );
  assertEquals(
    harness.events.removed.filter((type) => type !== "unload").join(","),
    TAB_STACKS_REBUILD_EVENTS.join(","),
    "explicit destroy should remove every exact native event listener",
  );
}

function testLoadingDocumentListenerCleanup(): void {
  const testDocument = createTestDocument();
  setProperty(testDocument, "readyState", "loading");

  const documentListenersAdded: string[] = [];
  const documentListenersRemoved: string[] = [];
  const originalAddEventListener = testDocument.addEventListener.bind(
    testDocument,
  );
  const originalRemoveEventListener = testDocument.removeEventListener.bind(
    testDocument,
  );
  setProperty(
    testDocument,
    "addEventListener",
    (
      type: string,
      callback: EventListenerOrEventListenerObject,
      options?: boolean | AddEventListenerOptions,
    ): void => {
      documentListenersAdded.push(type);
      originalAddEventListener(type, callback, options);
    },
  );
  setProperty(
    testDocument,
    "removeEventListener",
    (
      type: string,
      callback: EventListenerOrEventListenerObject,
      options?: boolean | EventListenerOptions,
    ): void => {
      documentListenersRemoved.push(type);
      originalRemoveEventListener(type, callback, options);
    },
  );

  const services = new FakeServices();
  services.values.set(TAB_STACKS_ENABLED_PREF, true);
  services.values.set(VERTICAL_TABS_PREF, false);
  const controller = initializeTabStacksFoundation({
    document: testDocument,
    eventTarget: new TrackingEventTarget(),
    services,
    getBrowser: () => null,
  });
  assert(controller, "enabled loading document should create a controller");
  assertEquals(
    documentListenersAdded.filter((type) => type === "DOMContentLoaded")
      .length,
    1,
    "loading startup should attach one DOMContentLoaded listener",
  );

  controller.destroy();
  assertEquals(
    documentListenersRemoved.filter((type) => type === "DOMContentLoaded")
      .length,
    1,
    "destroy before DOM readiness should remove the DOM listener",
  );
}

const tests: TestCase[] = [
  {
    name: "missing pref is inert and defaults false",
    fn: testMissingPrefIsInert,
  },
  {
    name: "eligible native group renders accessible read-only proxies",
    fn: testPresentationAndAccessibility,
  },
  {
    name: "eligibility, unnamed fallback, and split exclusion use live state",
    fn: testEligibilityFallbackAndSplitExclusion,
  },
  {
    name: "click and keyboard activate only existing native tabs",
    fn: testClickAndKeyboardSelection,
  },
  {
    name: "vertical lifecycle and explicit destroy clean all owned state",
    fn: testVerticalLifecycleAndExplicitDestroy,
  },
  {
    name: "loading-document teardown removes its DOM readiness listener",
    fn: testLoadingDocumentListenerCleanup,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("tabStacksFoundation.test.ts", tests);
}
