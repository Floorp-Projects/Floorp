// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../test/utils/test_harness.ts";
import {
  destroyTabDrop,
  initTabDrop,
  isTabDragToSplitCreationEnabled,
} from "../split-view-tab-drop.ts";
import {
  captureNativeTabDragRecovery,
  type DragSessionReader,
  type NativeTabDragController,
  type NativeTabDragRecovery,
  type RecoverableTab,
  recoverLostNativeTabDrag,
} from "../native-tab-drag-recovery.ts";
import type { SplitViewGBrowser, SplitViewWrapper } from "../../data/types.ts";

const TAB_DROP_TYPE = "application/x-moz-tabbrowser-tab";
const NEW_WINDOW_ZONE_ID = "floorp-new-window-drop-zone";
const PREF_SPLIT_VIEW_DND_CREATION_ENABLED =
  "floorp.splitView.dragToSplitCreate.enabled";

type TestTab = RecoverableTab & { _lastAccessed?: number };

interface DragScenario {
  addTabSplitViewCalls: () => number;
  cleanup: () => void;
  dispatchDragEnd: () => void;
  dispatchDragStartAndOver: () => void;
  dispatchDragToContent: () => { drop: Event };
  dispatchDropToContent: () => Event;
  draggedTab: TestTab;
  eventOrder: () => string[];
  nativeFinalizerCalls: () => string[];
  tabpanels: HTMLElement;
}

interface DragScenarioOptions {
  multiSelected?: boolean;
  readNativeDragSession?: DragSessionReader;
}

const testLogger = {
  debug() {},
  warn() {},
  error() {},
} as ConsoleInstance;

function restoreDragToSplitPref(
  hadUserValue: boolean,
  originalValue: boolean,
): void {
  if (hadUserValue) {
    Services.prefs.setBoolPref(
      PREF_SPLIT_VIEW_DND_CREATION_ENABLED,
      originalValue,
    );
  } else if (
    Services.prefs.prefHasUserValue(PREF_SPLIT_VIEW_DND_CREATION_ENABLED)
  ) {
    Services.prefs.clearUserPref(PREF_SPLIT_VIEW_DND_CREATION_ENABLED);
  }
}

function withRestoredPref(run: () => void): void {
  const hadUserValue = Services.prefs.prefHasUserValue(
    PREF_SPLIT_VIEW_DND_CREATION_ENABLED,
  );
  const originalValue = Services.prefs.getBoolPref(
    PREF_SPLIT_VIEW_DND_CREATION_ENABLED,
    true,
  );

  try {
    run();
  } finally {
    restoreDragToSplitPref(hadUserValue, originalValue);
  }
}

function createTestTab(label: string, lastAccessed: number): TestTab {
  return {
    linkedBrowser: document.createElement("browser") as unknown as XULElement,
    linkedPanel: `${label}-panel`,
    splitview: null,
    selected: false,
    label,
    _lastAccessed: lastAccessed,
  };
}

function createTabDragEvent(
  type: string,
  clientX = 100,
  clientY = 50,
  draggedTab: TestTab | null = null,
): Event {
  const event = new Event(type, { bubbles: true, cancelable: true });
  const dataTransfer = {
    types: [TAB_DROP_TYPE],
    dropEffect: "none",
    mozGetDataAt: () => draggedTab,
  };

  Object.defineProperty(event, "dataTransfer", {
    value: dataTransfer,
    configurable: true,
  });
  Object.defineProperty(event, "clientX", {
    value: clientX,
    configurable: true,
  });
  Object.defineProperty(event, "clientY", {
    value: clientY,
    configurable: true,
  });

  return event;
}

function setGlobalGBrowser(value: SplitViewGBrowser): () => void {
  const originalDescriptor = Object.getOwnPropertyDescriptor(
    globalThis,
    "gBrowser",
  );

  Object.defineProperty(globalThis, "gBrowser", {
    value,
    configurable: true,
  });

  return () => {
    if (originalDescriptor) {
      Object.defineProperty(globalThis, "gBrowser", originalDescriptor);
      return;
    }

    delete (globalThis as Record<string, unknown>).gBrowser;
  };
}

function ensureTabpanels(): {
  element: HTMLElement;
  cleanup: () => void;
} {
  const existing = document.getElementById(
    "tabbrowser-tabpanels",
  ) as HTMLElement | null;
  const element = existing ?? document.createElement("div");
  const created = existing === null;

  if (created) {
    element.id = "tabbrowser-tabpanels";
    document.documentElement!.appendChild(element);
  }

  const originalRect = Object.getOwnPropertyDescriptor(
    element,
    "getBoundingClientRect",
  );
  Object.defineProperty(element, "getBoundingClientRect", {
    value: () => ({
      bottom: 100,
      height: 100,
      left: 0,
      right: 200,
      top: 0,
      width: 200,
      x: 0,
      y: 0,
      toJSON: () => ({}),
    }),
    configurable: true,
  });

  return {
    element,
    cleanup: () => {
      element.removeAttribute("data-floorp-tab-dragging");
      element.removeAttribute("split-view-layout");
      if (originalRect) {
        Object.defineProperty(element, "getBoundingClientRect", originalRect);
      } else {
        delete (element as unknown as Record<string, unknown>)
          .getBoundingClientRect;
      }
      if (created) {
        element.remove();
      }
    },
  };
}

function markMovingTab(id: string): {
  element: Element;
  cleanup: () => void;
} {
  const existing = document.getElementById(id) as HTMLElement | null;
  const element = existing ?? document.createElement("div");
  const created = existing === null;
  const originalValue = element.getAttribute("movingtab");

  if (created) {
    element.id = id;
    document.documentElement!.appendChild(element);
  }
  element.setAttribute("movingtab", "true");

  return {
    element,
    cleanup: () => {
      if (created) {
        element.remove();
      } else if (originalValue === null) {
        element.removeAttribute("movingtab");
      } else {
        element.setAttribute("movingtab", originalValue);
      }
    },
  };
}

function setupDragScenario(options: DragScenarioOptions = {}): DragScenario {
  destroyTabDrop();

  const { element: tabpanels, cleanup: cleanupTabpanels } = ensureTabpanels();
  const tabContainer = document.createElement("div") as HTMLElement & {
    linkedTab?: TestTab;
  };
  const draggedTab = createTestTab("dragged", 1);
  const partnerTab = createTestTab("partner", 10);
  const selectedPeer = createTestTab("selected-peer", 5);
  if (options.multiSelected) {
    draggedTab.selected = true;
    selectedPeer.selected = true;
  }
  let addTabSplitViewCalls = 0;
  const eventOrder: string[] = [];
  const nativeFinalizerCalls: string[] = [];

  const nativeController: NativeTabDragController = {
    finishMoveTogetherSelectedTabs() {
      nativeFinalizerCalls.push("finishMoveTogetherSelectedTabs");
    },
    finishAnimateTabMove() {
      nativeFinalizerCalls.push("finishAnimateTabMove");
    },
    _resetTabsAfterDrop() {
      nativeFinalizerCalls.push("_resetTabsAfterDrop");
    },
  };
  draggedTab._dragData = {};
  draggedTab.container = { tabDragAndDrop: nativeController };
  draggedTab.ownerDocument = document;

  tabContainer.linkedTab = draggedTab;
  document.documentElement!.appendChild(tabContainer);
  tabContainer.addEventListener("dragend", () => {
    eventOrder.push("gecko-dragend");
  });

  const wrapper: SplitViewWrapper = {
    tabs: [partnerTab, draggedTab],
    addTabs() {},
    reverseTabs() {},
    unsplitTabs() {},
  };

  const mockGBrowser: SplitViewGBrowser = {
    tabpanels: tabpanels as unknown as SplitViewGBrowser["tabpanels"],
    tabContainer: tabContainer as unknown as XULElement,
    tabs: [partnerTab, selectedPeer, draggedTab],
    selectedTab: partnerTab,
    selectedTabs: options.multiSelected ? [draggedTab, selectedPeer] : [],
    activeSplitView: null,
    showSplitViewPanels() {},
    moveTabBefore() {},
    moveTabToSplitView() {},
    addTrustedTab() {
      return createTestTab("trusted", 20);
    },
    addTabSplitView() {
      addTabSplitViewCalls += 1;
      eventOrder.push("split-created");
      return wrapper;
    },
    replaceTabWithWindow() {},
  };

  const cleanupGBrowser = setGlobalGBrowser(mockGBrowser);
  initTabDrop(testLogger, options.readNativeDragSession);

  const dispatchDragStartAndOver = (): void => {
    tabContainer.dispatchEvent(
      createTabDragEvent("dragstart", 100, 50, draggedTab),
    );
    document.dispatchEvent(
      createTabDragEvent("dragover", 100, 50, draggedTab),
    );
  };
  const dispatchDropToContent = (): Event => {
    const drop = createTabDragEvent("drop", 100, 50, draggedTab);
    document.dispatchEvent(drop);
    return drop;
  };
  const dispatchDragEnd = (): void => {
    tabContainer.dispatchEvent(
      createTabDragEvent("dragend", 100, 50, draggedTab),
    );
  };

  return {
    addTabSplitViewCalls: () => addTabSplitViewCalls,
    draggedTab,
    eventOrder: () => [...eventOrder],
    nativeFinalizerCalls: () => [...nativeFinalizerCalls],
    tabpanels,
    dispatchDragEnd,
    dispatchDragStartAndOver,
    dispatchDropToContent,
    dispatchDragToContent: () => {
      dispatchDragStartAndOver();
      const drop = dispatchDropToContent();
      dispatchDragEnd();
      return { drop };
    },
    cleanup: () => {
      destroyTabDrop();
      cleanupGBrowser();
      cleanupTabpanels();
      tabContainer.remove();
      document.getElementById(NEW_WINDOW_ZONE_ID)?.remove();
    },
  };
}

function createRecoveryFixture(
  controller: NativeTabDragController,
  calls: string[],
): { tab: TestTab; recovery: NativeTabDragRecovery } {
  const rawTab = createTestTab("recoverable", 1);
  rawTab._dragData = {};
  rawTab.container = { tabDragAndDrop: controller };
  rawTab.ownerDocument = document;

  const tab = new Proxy(rawTab, {
    deleteProperty(target, property) {
      if (property === "_dragData") {
        calls.push("delete _dragData");
      }
      return Reflect.deleteProperty(target, property);
    },
  });
  const recovery = captureNativeTabDragRecovery(tab);
  assert(recovery !== null, "recovery fixture should capture native identity");
  return { tab, recovery };
}

function waitForLostTerminalRecovery(): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, 150));
}

function waitForRecoveryRetry(): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, 350));
}

function waitForRecoveryDeadline(): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, 1250));
}

async function withEnabledDragScenario(
  run: (scenario: DragScenario) => Promise<void> | void,
  options: DragScenarioOptions = {},
): Promise<void> {
  const hadUserValue = Services.prefs.prefHasUserValue(
    PREF_SPLIT_VIEW_DND_CREATION_ENABLED,
  );
  const originalValue = Services.prefs.getBoolPref(
    PREF_SPLIT_VIEW_DND_CREATION_ENABLED,
    true,
  );
  Services.prefs.setBoolPref(PREF_SPLIT_VIEW_DND_CREATION_ENABLED, true);
  const scenario = setupDragScenario(options);

  try {
    await run(scenario);
  } finally {
    scenario.cleanup();
    restoreDragToSplitPref(hadUserValue, originalValue);
  }
}

type SafetyTerminal = "mouseup" | "blur" | "hidden" | "dragged-tab-close";

function dispatchSafetyTerminal(
  scenario: DragScenario,
  terminal: SafetyTerminal,
): void {
  if (terminal === "hidden") {
    const originalDescriptor = Object.getOwnPropertyDescriptor(
      document,
      "hidden",
    );
    Object.defineProperty(document, "hidden", {
      value: true,
      configurable: true,
    });
    try {
      document.dispatchEvent(new Event("visibilitychange"));
    } finally {
      if (originalDescriptor) {
        Object.defineProperty(document, "hidden", originalDescriptor);
      } else {
        delete (document as unknown as Record<string, unknown>).hidden;
      }
    }
    return;
  }

  if (terminal === "dragged-tab-close") {
    const event = new Event("TabClose", { bubbles: true });
    Object.defineProperty(event, "target", {
      value: scenario.draggedTab,
      configurable: true,
    });
    globalThis.dispatchEvent(event);
    return;
  }

  globalThis.dispatchEvent(new Event(terminal));
}

function testTabDragToSplitCreationDefaultsToEnabled(): void {
  withRestoredPref(() => {
    if (
      Services.prefs.prefHasUserValue(PREF_SPLIT_VIEW_DND_CREATION_ENABLED)
    ) {
      Services.prefs.clearUserPref(PREF_SPLIT_VIEW_DND_CREATION_ENABLED);
    }

    assertEquals(
      isTabDragToSplitCreationEnabled(),
      true,
      "tab drag-to-split creation should default to enabled",
    );
  });
}

function testTabDragToSplitCreationCanBeDisabled(): void {
  withRestoredPref(() => {
    Services.prefs.setBoolPref(PREF_SPLIT_VIEW_DND_CREATION_ENABLED, false);

    assertEquals(
      isTabDragToSplitCreationEnabled(),
      false,
      "tab drag-to-split creation should be disabled when the pref is false",
    );
  });
}

function testTabDragToSplitCreationCanBeEnabled(): void {
  withRestoredPref(() => {
    Services.prefs.setBoolPref(PREF_SPLIT_VIEW_DND_CREATION_ENABLED, true);

    assertEquals(
      isTabDragToSplitCreationEnabled(),
      true,
      "tab drag-to-split creation should be enabled when the pref is true",
    );
  });
}

function testDisabledPrefPreventsDragDropSplitCreation(): void {
  withRestoredPref(() => {
    Services.prefs.setBoolPref(PREF_SPLIT_VIEW_DND_CREATION_ENABLED, false);
    const scenario = setupDragScenario();

    try {
      const { drop } = scenario.dispatchDragToContent();

      assertEquals(
        scenario.addTabSplitViewCalls(),
        0,
        "disabled pref should prevent drag-and-drop split view creation",
      );
      assertEquals(
        drop.defaultPrevented,
        false,
        "disabled pref should not claim the content drop target",
      );
      assert(
        !scenario.tabpanels.hasAttribute("data-floorp-tab-dragging"),
        "disabled pref should not leave tab dragging state on tabpanels",
      );
      assertEquals(
        document.getElementById(NEW_WINDOW_ZONE_ID),
        null,
        "disabled pref should not create the new-window drop zone",
      );
    } finally {
      scenario.cleanup();
    }
  });
}

function testEnabledPrefAllowsDragDropSplitCreation(): void {
  withRestoredPref(() => {
    Services.prefs.setBoolPref(PREF_SPLIT_VIEW_DND_CREATION_ENABLED, true);
    const scenario = setupDragScenario();

    try {
      const { drop } = scenario.dispatchDragToContent();

      assertEquals(
        scenario.addTabSplitViewCalls(),
        1,
        "enabled pref should allow drag-and-drop split view creation",
      );
      assertEquals(
        drop.defaultPrevented,
        true,
        "enabled pref should claim the content drop target",
      );
    } finally {
      scenario.cleanup();
    }
  });
}

async function testNormalDragEndLetsGeckoFinishFirst(): Promise<void> {
  const hadUserValue = Services.prefs.prefHasUserValue(
    PREF_SPLIT_VIEW_DND_CREATION_ENABLED,
  );
  const originalValue = Services.prefs.getBoolPref(
    PREF_SPLIT_VIEW_DND_CREATION_ENABLED,
    true,
  );
  Services.prefs.setBoolPref(PREF_SPLIT_VIEW_DND_CREATION_ENABLED, true);

  const scenario = setupDragScenario();
  const tabstrip = markMovingTab("tabbrowser-tabs");
  const toolbox = markMovingTab("navigator-toolbox");

  try {
    scenario.dispatchDragToContent();
    await waitForLostTerminalRecovery();

    assertEquals(
      scenario.eventOrder().join(" > "),
      "gecko-dragend > split-created",
      "bubbling dragend should reach Gecko before Floorp creates the split",
    );
    assertEquals(
      scenario.nativeFinalizerCalls().length,
      0,
      "normal dragend must not invoke private native finalizers",
    );
    assert(
      scenario.draggedTab._dragData !== undefined,
      "normal Floorp cleanup must not delete Gecko _dragData",
    );
    assert(
      tabstrip.element.hasAttribute("movingtab"),
      "normal Floorp cleanup must not remove tabstrip movingtab",
    );
    assert(
      toolbox.element.hasAttribute("movingtab"),
      "normal Floorp cleanup must not remove toolbox movingtab",
    );
  } finally {
    toolbox.cleanup();
    tabstrip.cleanup();
    scenario.cleanup();
    restoreDragToSplitPref(hadUserValue, originalValue);
  }
}

async function testNormalMultiSelectedDragEndDoesNotRecoverPrivately(): Promise<
  void
> {
  await withEnabledDragScenario(async (scenario) => {
    scenario.dispatchDragToContent();
    await waitForLostTerminalRecovery();

    assertEquals(
      scenario.eventOrder().join(" > "),
      "gecko-dragend > split-created",
      "normal multi-selected dragend should remain Gecko-first",
    );
    assertEquals(
      scenario.nativeFinalizerCalls().length,
      0,
      "Floorp must not repeat multi-selected finalization after normal dragend",
    );
  }, { multiSelected: true });
}

async function testLostContentDropRecoversThenCreatesSplit(): Promise<void> {
  await withEnabledDragScenario(async (scenario) => {
    scenario.dispatchDragStartAndOver();
    const drop = scenario.dispatchDropToContent();

    assertEquals(
      drop.defaultPrevented,
      true,
      "the accepted content drop should be the split-creation source",
    );
    assertEquals(
      scenario.addTabSplitViewCalls(),
      0,
      "a lost dragend path must wait for guarded native recovery",
    );
    assert(
      !scenario.tabpanels.hasAttribute("data-floorp-tab-dragging"),
      "Floorp UI should clear before the recovery grace period",
    );

    await waitForLostTerminalRecovery();

    assertEquals(
      scenario.nativeFinalizerCalls().join(" > "),
      "finishMoveTogetherSelectedTabs > finishAnimateTabMove > " +
        "_resetTabsAfterDrop",
      "a proven lost content drop should use the exact native order",
    );
    assertEquals(
      scenario.addTabSplitViewCalls(),
      1,
      "only full recovery of an accepted content drop may create a split",
    );
  });
}

async function testActiveSessionRetriesThenCreatesOneSplit(): Promise<void> {
  const activeSession = {} as nsIDragSession;
  let sessionReads = 0;
  await withEnabledDragScenario(async (scenario) => {
    scenario.dispatchDragStartAndOver();
    scenario.dispatchDropToContent();

    await waitForRecoveryRetry();

    assertEquals(
      sessionReads,
      2,
      "recovery should retry after the first active native session",
    );
    assertEquals(
      scenario.nativeFinalizerCalls().join(" > "),
      "finishMoveTogetherSelectedTabs > finishAnimateTabMove > " +
        "_resetTabsAfterDrop",
      "the later session-null attempt should finalize the native drag once",
    );
    assertEquals(
      scenario.addTabSplitViewCalls(),
      1,
      "the accepted content drop should survive one active-session retry",
    );
  }, {
    readNativeDragSession: () => {
      sessionReads += 1;
      return sessionReads === 1 ? activeSession : null;
    },
  });
}

async function testActiveSessionDeadlineDiscardsWithoutResurrection(): Promise<
  void
> {
  const activeSession = {} as nsIDragSession;
  let sessionActive = true;
  let sessionReads = 0;
  await withEnabledDragScenario(async (scenario) => {
    scenario.dispatchDragStartAndOver();
    scenario.dispatchDropToContent();

    await waitForRecoveryDeadline();

    assert(
      sessionReads > 1,
      "an active session should be retried before the bounded deadline",
    );
    assertEquals(
      scenario.nativeFinalizerCalls().length,
      0,
      "an active session through the deadline must not run native finalizers",
    );
    assertEquals(
      scenario.addTabSplitViewCalls(),
      0,
      "deadline expiry must discard the accepted pending split",
    );

    sessionActive = false;
    await waitForRecoveryRetry();
    scenario.dispatchDragEnd();
    await waitForLostTerminalRecovery();

    assertEquals(
      scenario.nativeFinalizerCalls().length,
      0,
      "session-null after expiry must not resurrect the discarded transaction",
    );
    assertEquals(
      scenario.addTabSplitViewCalls(),
      0,
      "a late dragend must not resurrect the discarded pending split",
    );
  }, {
    readNativeDragSession: () => {
      sessionReads += 1;
      return sessionActive ? activeSession : null;
    },
  });
}

async function testLostContentDropWithClosingTabNeverCreatesSplit(): Promise<
  void
> {
  await withEnabledDragScenario(async (scenario) => {
    scenario.dispatchDragStartAndOver();
    scenario.dispatchDropToContent();
    dispatchSafetyTerminal(scenario, "dragged-tab-close");
    await waitForLostTerminalRecovery();

    assertEquals(
      scenario.nativeFinalizerCalls().join(" > "),
      "finishAnimateTabMove > _resetTabsAfterDrop",
      "a closing content-drop tab should receive controller-only UI recovery",
    );
    assertEquals(
      scenario.addTabSplitViewCalls(),
      0,
      "UI-only recovery must discard even an accepted pending split",
    );
  });
}

async function testBlockedContentDropRecoveryNeverCreatesSplit(): Promise<
  void
> {
  await withEnabledDragScenario(async (scenario) => {
    scenario.dispatchDragStartAndOver();
    scenario.dispatchDropToContent();
    scenario.draggedTab.container = {
      tabDragAndDrop: {
        finishMoveTogetherSelectedTabs() {},
        finishAnimateTabMove() {},
        _resetTabsAfterDrop() {},
      },
    };
    await waitForLostTerminalRecovery();

    assertEquals(
      scenario.nativeFinalizerCalls().length,
      0,
      "changed native identity must block all captured-controller finalizers",
    );
    assertEquals(
      scenario.addTabSplitViewCalls(),
      0,
      "blocked recovery must discard an accepted pending split",
    );
  });
}

async function testSafetyTerminalsRecoverWithoutCreatingSplit(): Promise<void> {
  const terminals: SafetyTerminal[] = [
    "mouseup",
    "blur",
    "hidden",
    "dragged-tab-close",
  ];

  for (const terminal of terminals) {
    await withEnabledDragScenario(async (scenario) => {
      scenario.dispatchDragStartAndOver();
      dispatchSafetyTerminal(scenario, terminal);

      assert(
        !scenario.tabpanels.hasAttribute("data-floorp-tab-dragging"),
        `${terminal} should clear Floorp UI synchronously`,
      );

      await waitForLostTerminalRecovery();

      const expectedFinalizers = terminal === "dragged-tab-close"
        ? "finishAnimateTabMove > _resetTabsAfterDrop"
        : "finishMoveTogetherSelectedTabs > finishAnimateTabMove > " +
          "_resetTabsAfterDrop";
      assertEquals(
        scenario.nativeFinalizerCalls().join(" > "),
        expectedFinalizers,
        `${terminal} should use the guarded terminal-specific recovery path`,
      );
      assertEquals(
        scenario.addTabSplitViewCalls(),
        0,
        `${terminal} without an accepted content drop must never create a split`,
      );
    });
  }
}

async function testStuckWatchdogRecoversWithoutCreatingSplit(): Promise<void> {
  await withEnabledDragScenario(async (scenario) => {
    scenario.dispatchDragStartAndOver();

    await new Promise((resolve) => setTimeout(resolve, 2250));

    assert(
      !scenario.tabpanels.hasAttribute("data-floorp-tab-dragging"),
      "the watchdog should restore content input before recovery settles",
    );
    assertEquals(
      scenario.nativeFinalizerCalls().join(" > "),
      "finishMoveTogetherSelectedTabs > finishAnimateTabMove > " +
        "_resetTabsAfterDrop",
      "the watchdog should schedule the same guarded native recovery",
    );
    assertEquals(
      scenario.addTabSplitViewCalls(),
      0,
      "watchdog-only recovery must never create a split",
    );
  });
}

async function testLostMultiSelectedDropFinalizesOnceBeforeSplit(): Promise<
  void
> {
  await withEnabledDragScenario(async (scenario) => {
    scenario.dispatchDragStartAndOver();
    scenario.dispatchDropToContent();
    await waitForLostTerminalRecovery();

    assertEquals(
      scenario.nativeFinalizerCalls().join(" > "),
      "finishMoveTogetherSelectedTabs > finishAnimateTabMove > " +
        "_resetTabsAfterDrop",
      "lost multi-selected recovery should finalize the group exactly once first",
    );
    assertEquals(
      scenario.addTabSplitViewCalls(),
      1,
      "a fully recovered multi-selected content drop may create one split",
    );
  }, { multiSelected: true });
}

async function testMultiSelectedSafetyTerminalNeverCreatesSplit(): Promise<
  void
> {
  await withEnabledDragScenario(async (scenario) => {
    scenario.dispatchDragStartAndOver();
    dispatchSafetyTerminal(scenario, "mouseup");
    await waitForLostTerminalRecovery();

    assertEquals(
      scenario.nativeFinalizerCalls().filter((call) =>
        call === "finishMoveTogetherSelectedTabs"
      ).length,
      1,
      "multi-selected safety recovery should finalize the group once",
    );
    assertEquals(
      scenario.addTabSplitViewCalls(),
      0,
      "multi-selected safety-only recovery must never create a split",
    );
  }, { multiSelected: true });
}

function testNullSessionRecoveryUsesExactOrderOnce(): void {
  const calls: string[] = [];
  const controller: NativeTabDragController = {
    finishMoveTogetherSelectedTabs() {
      calls.push("finishMoveTogetherSelectedTabs");
    },
    finishAnimateTabMove() {
      calls.push("finishAnimateTabMove");
    },
    _resetTabsAfterDrop() {
      calls.push("_resetTabsAfterDrop");
    },
  };
  const { tab, recovery } = createRecoveryFixture(controller, calls);

  assertEquals(
    recoverLostNativeTabDrag(recovery, recovery, () => {
      calls.push("readSession");
      return null;
    }),
    "full",
    "null native session should permit strict lost-dragend recovery",
  );
  assertEquals(
    recoverLostNativeTabDrag(recovery, recovery, () => null),
    "blocked",
    "terminal guard should reject a repeated recovery",
  );
  assertEquals(
    calls.join(" > "),
    "readSession > finishMoveTogetherSelectedTabs > finishAnimateTabMove > " +
      "_resetTabsAfterDrop > delete _dragData",
    "native recovery should prove session-null before the exact sequence",
  );
  assertEquals(
    tab._dragData,
    undefined,
    "successful recovery should delete the captured _dragData last",
  );
}

function testActiveSessionIsRetryableWithoutConsumingGuard(): void {
  const calls: string[] = [];
  const controller: NativeTabDragController = {
    finishMoveTogetherSelectedTabs() {
      calls.push("finishMoveTogetherSelectedTabs");
    },
    finishAnimateTabMove() {
      calls.push("finishAnimateTabMove");
    },
    _resetTabsAfterDrop() {
      calls.push("_resetTabsAfterDrop");
    },
  };
  const { tab, recovery } = createRecoveryFixture(controller, calls);
  const activeSession = {} as nsIDragSession;

  assertEquals(
    recoverLostNativeTabDrag(recovery, recovery, () => activeSession),
    "active-session",
    "an active drag session should return the retryable result",
  );
  assertEquals(
    calls.length,
    0,
    "active-session rejection must not partially touch Gecko state",
  );
  assert(
    tab._dragData === recovery.dragData,
    "active-session rejection should preserve captured _dragData",
  );
  assertEquals(
    recovery.terminalGuardUsed,
    false,
    "an active-session result must not consume the terminal guard",
  );
  assertEquals(
    recoverLostNativeTabDrag(recovery, recovery, () => null),
    "full",
    "a later session-null attempt should still be allowed to recover",
  );
  assertEquals(
    calls.join(" > "),
    "finishMoveTogetherSelectedTabs > finishAnimateTabMove > " +
      "_resetTabsAfterDrop > delete _dragData",
    "the retryable attempt must leave the exact finalizer sequence untouched",
  );
}

function testSessionReaderFailureBlocksBeforePrivateFinalizers(): void {
  const calls: string[] = [];
  const controller: NativeTabDragController = {
    finishMoveTogetherSelectedTabs() {
      calls.push("finishMoveTogetherSelectedTabs");
    },
    finishAnimateTabMove() {
      calls.push("finishAnimateTabMove");
    },
    _resetTabsAfterDrop() {
      calls.push("_resetTabsAfterDrop");
    },
  };
  const { tab, recovery } = createRecoveryFixture(controller, calls);

  assertEquals(
    recoverLostNativeTabDrag(recovery, recovery, () => {
      throw new Error("drag service unavailable");
    }),
    "blocked",
    "a failed native-session read must block private recovery",
  );
  assertEquals(
    calls.length,
    0,
    "a failed session read must happen before every private finalizer",
  );
  assert(
    tab._dragData === recovery.dragData,
    "a failed session read should preserve captured _dragData",
  );
}

function testClosingTabWithMissingContainerUsesCapturedController(): void {
  const calls: string[] = [];
  const controller: NativeTabDragController = {
    // A gone tab must not require or invoke finishMoveTogetherSelectedTabs.
    finishAnimateTabMove() {
      calls.push("finishAnimateTabMove");
    },
    _resetTabsAfterDrop() {
      calls.push("_resetTabsAfterDrop");
    },
  };
  const { tab, recovery } = createRecoveryFixture(controller, calls);
  tab.closing = true;
  tab.container = undefined;

  assertEquals(
    recoverLostNativeTabDrag(recovery, recovery, () => {
      calls.push("readSession");
      return null;
    }),
    "ui-only-tab-gone",
    "a closing tab may use its captured controller after container removal",
  );
  assertEquals(
    calls.join(" > "),
    "readSession > finishAnimateTabMove > _resetTabsAfterDrop > " +
      "delete _dragData",
    "gone-tab recovery must skip the tab-dependent native finalizer",
  );
  assertEquals(
    tab._dragData,
    undefined,
    "gone-tab recovery should delete only the identical captured _dragData",
  );
}

function testDisconnectedTabWithChangedContainerUsesCapturedController(): void {
  const calls: string[] = [];
  const controller: NativeTabDragController = {
    finishAnimateTabMove() {
      calls.push("finishAnimateTabMove");
    },
    _resetTabsAfterDrop() {
      calls.push("_resetTabsAfterDrop");
    },
  };
  const { tab, recovery } = createRecoveryFixture(controller, calls);
  tab.isConnected = false;
  tab.container = {
    tabDragAndDrop: {
      finishAnimateTabMove() {
        calls.push("replacement finishAnimateTabMove");
      },
      _resetTabsAfterDrop() {
        calls.push("replacement _resetTabsAfterDrop");
      },
    },
  };

  assertEquals(
    recoverLostNativeTabDrag(recovery, recovery, () => null),
    "ui-only-tab-gone",
    "a disconnected tab may use its captured controller after replacement",
  );
  assertEquals(
    calls.join(" > "),
    "finishAnimateTabMove > _resetTabsAfterDrop > delete _dragData",
    "gone-tab recovery must invoke only the captured controller's exact calls",
  );
}

function testGoneTabStillRequiresCapturedDragData(): void {
  const calls: string[] = [];
  const controller: NativeTabDragController = {
    finishAnimateTabMove() {
      calls.push("finishAnimateTabMove");
    },
    _resetTabsAfterDrop() {
      calls.push("_resetTabsAfterDrop");
    },
  };
  const { tab, recovery } = createRecoveryFixture(controller, calls);
  const replacementDragData = {};
  tab.closing = true;
  tab.container = undefined;
  tab._dragData = replacementDragData;
  let sessionReads = 0;

  assertEquals(
    recoverLostNativeTabDrag(recovery, recovery, () => {
      sessionReads += 1;
      return null;
    }),
    "blocked",
    "gone-tab recovery must retain the captured native drag identity guard",
  );
  assertEquals(
    sessionReads,
    0,
    "a changed drag payload should fail before reading native session state",
  );
  assertEquals(
    calls.length,
    0,
    "a changed drag payload must not invoke the captured controller",
  );
  assert(
    tab._dragData === replacementDragData,
    "blocked recovery must preserve replacement drag data",
  );
}

function testMissingNativeApiFailsClosedBeforeSessionRead(): void {
  const calls: string[] = [];
  const controller: NativeTabDragController = {
    finishMoveTogetherSelectedTabs() {
      calls.push("finishMoveTogetherSelectedTabs");
    },
    // finishAnimateTabMove is intentionally absent.
    _resetTabsAfterDrop() {
      calls.push("_resetTabsAfterDrop");
    },
  };
  const { tab, recovery } = createRecoveryFixture(controller, calls);
  let sessionReads = 0;

  assertEquals(
    recoverLostNativeTabDrag(recovery, recovery, () => {
      sessionReads += 1;
      return null;
    }),
    "blocked",
    "missing native API should make recovery ineligible",
  );
  assertEquals(
    sessionReads,
    0,
    "all native APIs should be preflighted before reading terminal state",
  );
  assertEquals(
    calls.length,
    0,
    "missing API rejection must not partially invoke available finalizers",
  );
  assert(
    tab._dragData === recovery.dragData,
    "missing API rejection should preserve captured _dragData",
  );
}

function testRecoveryRejectsReentrancy(): void {
  const calls: string[] = [];
  let capturedRecovery: NativeTabDragRecovery | null = null;
  const controller: NativeTabDragController = {
    finishMoveTogetherSelectedTabs() {
      calls.push("finishMoveTogetherSelectedTabs");
      assert(capturedRecovery !== null, "fixture should assign recovery");
      const nested = recoverLostNativeTabDrag(
        capturedRecovery,
        capturedRecovery,
        () => null,
      );
      calls.push(`reentrant:${nested}`);
    },
    finishAnimateTabMove() {
      calls.push("finishAnimateTabMove");
    },
    _resetTabsAfterDrop() {
      calls.push("_resetTabsAfterDrop");
    },
  };
  const fixture = createRecoveryFixture(controller, calls);
  capturedRecovery = fixture.recovery;

  assertEquals(
    recoverLostNativeTabDrag(
      fixture.recovery,
      fixture.recovery,
      () => null,
    ),
    "full",
    "outer recovery should complete",
  );
  assertEquals(
    calls.join(" > "),
    "finishMoveTogetherSelectedTabs > reentrant:blocked > " +
      "finishAnimateTabMove > _resetTabsAfterDrop > delete _dragData",
    "terminal guard should block re-entry before the first finalizer",
  );
}

function testRecoveryRequiresCapturedIdentityAndUnobservedDragend(): void {
  const calls: string[] = [];
  const controller: NativeTabDragController = {
    finishMoveTogetherSelectedTabs() {
      calls.push("finishMoveTogetherSelectedTabs");
    },
    finishAnimateTabMove() {
      calls.push("finishAnimateTabMove");
    },
    _resetTabsAfterDrop() {
      calls.push("_resetTabsAfterDrop");
    },
  };
  const { tab, recovery } = createRecoveryFixture(controller, calls);
  const differentTransaction = { ...recovery };
  let sessionReads = 0;

  assertEquals(
    recoverLostNativeTabDrag(recovery, differentTransaction, () => {
      sessionReads += 1;
      return null;
    }),
    "blocked",
    "a different transaction object must fail identity validation",
  );
  recovery.dragendObserved = true;
  assertEquals(
    recoverLostNativeTabDrag(recovery, recovery, () => {
      sessionReads += 1;
      return null;
    }),
    "blocked",
    "an observed dragend must make recovery ineligible",
  );
  recovery.dragendObserved = false;
  tab._dragData = {};
  assertEquals(
    recoverLostNativeTabDrag(recovery, recovery, () => {
      sessionReads += 1;
      return null;
    }),
    "blocked",
    "a replaced native drag transaction must fail tab identity validation",
  );
  tab._dragData = recovery.dragData;
  tab.container = {
    tabDragAndDrop: {
      finishMoveTogetherSelectedTabs() {},
      finishAnimateTabMove() {},
      _resetTabsAfterDrop() {},
    },
  };
  assertEquals(
    recoverLostNativeTabDrag(recovery, recovery, () => {
      sessionReads += 1;
      return null;
    }),
    "blocked",
    "a changed native controller must fail captured identity validation",
  );
  assertEquals(
    sessionReads,
    0,
    "identity mismatch should fail before querying the drag service",
  );
  assertEquals(
    calls.length,
    0,
    "identity mismatch must not touch Gecko state",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "tab drag-to-split creation defaults to enabled",
      fn: testTabDragToSplitCreationDefaultsToEnabled,
    },
    {
      name: "tab drag-to-split creation can be disabled",
      fn: testTabDragToSplitCreationCanBeDisabled,
    },
    {
      name: "tab drag-to-split creation can be enabled",
      fn: testTabDragToSplitCreationCanBeEnabled,
    },
    {
      name: "disabled pref prevents drag-and-drop split view creation",
      fn: testDisabledPrefPreventsDragDropSplitCreation,
    },
    {
      name: "enabled pref allows drag-and-drop split view creation",
      fn: testEnabledPrefAllowsDragDropSplitCreation,
    },
    {
      name: "normal bubbling dragend lets Gecko finish before Floorp",
      fn: testNormalDragEndLetsGeckoFinishFirst,
    },
    {
      name: "normal multi-selected dragend does not use private recovery",
      fn: testNormalMultiSelectedDragEndDoesNotRecoverPrivately,
    },
    {
      name: "lost content drop recovers before creating split",
      fn: testLostContentDropRecoversThenCreatesSplit,
    },
    {
      name: "active session retries then creates one split",
      fn: testActiveSessionRetriesThenCreatesOneSplit,
    },
    {
      name: "active session deadline discards without resurrection",
      fn: testActiveSessionDeadlineDiscardsWithoutResurrection,
    },
    {
      name: "lost content drop with closing tab never creates split",
      fn: testLostContentDropWithClosingTabNeverCreatesSplit,
    },
    {
      name: "blocked content-drop recovery never creates split",
      fn: testBlockedContentDropRecoveryNeverCreatesSplit,
    },
    {
      name: "safety terminals recover without creating split",
      fn: testSafetyTerminalsRecoverWithoutCreatingSplit,
    },
    {
      name: "stuck watchdog recovers without creating split",
      fn: testStuckWatchdogRecoversWithoutCreatingSplit,
    },
    {
      name: "lost multi-selected drop finalizes once before split",
      fn: testLostMultiSelectedDropFinalizesOnceBeforeSplit,
    },
    {
      name: "multi-selected safety terminal never creates split",
      fn: testMultiSelectedSafetyTerminalNeverCreatesSplit,
    },
    {
      name: "null-session recovery uses exact native order once",
      fn: testNullSessionRecoveryUsesExactOrderOnce,
    },
    {
      name: "active native session is retryable without consuming guard",
      fn: testActiveSessionIsRetryableWithoutConsumingGuard,
    },
    {
      name: "session read failure blocks before private finalizers",
      fn: testSessionReaderFailureBlocksBeforePrivateFinalizers,
    },
    {
      name: "closing tab with missing container uses captured controller",
      fn: testClosingTabWithMissingContainerUsesCapturedController,
    },
    {
      name: "disconnected tab with changed container uses captured controller",
      fn: testDisconnectedTabWithChangedContainerUsesCapturedController,
    },
    {
      name: "gone tab still requires captured drag data",
      fn: testGoneTabStillRequiresCapturedDragData,
    },
    {
      name: "missing native API fails closed before session read",
      fn: testMissingNativeApiFailsClosedBeforeSessionRead,
    },
    {
      name: "native recovery rejects reentrancy",
      fn: testRecoveryRejectsReentrancy,
    },
    {
      name: "native recovery requires captured identity and lost dragend",
      fn: testRecoveryRequiresCapturedIdentityAndUnobservedDragend,
    },
  ];

  await runTests("split-view-tab-drop.test.ts", tests);
}
