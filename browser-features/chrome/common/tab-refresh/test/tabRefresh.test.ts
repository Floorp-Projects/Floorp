// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  ENABLED_PREF,
  GLYPH_CLASS,
  HoverReloadController,
  installHoverReloadController,
  ROOT_ATTR,
  STAMP_ATTR,
  STYLE_MARKER_ATTR,
  uninstallHoverReloadController,
} from "../controller.ts";
import type {
  HoverReloadBrowser,
  HoverReloadClock,
  HoverReloadControllerOptions,
  HoverReloadDocument,
  HoverReloadMutationObserver,
  HoverReloadPrefObserver,
  HoverReloadPrefs,
  HoverReloadTab,
  HoverReloadTimerHandle,
} from "../types.ts";
import {
  assert as harnessAssert,
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function assert(
  condition: unknown,
  message = "expected condition to be truthy",
): asserts condition {
  harnessAssert(condition, message);
}

function assertEquals<T>(
  actual: T,
  expected: T,
  message = "expected values to be equal",
): void {
  harnessAssertEquals(actual, expected, message);
}

function browserRoot(): Element {
  const root = document.documentElement;
  assert(root !== null, "browser document should have a root element");
  return root;
}

class FakePrefs implements HoverReloadPrefs {
  private readonly observers = new Set<HoverReloadPrefObserver>();

  constructor(private value: boolean) {}

  get observerCount(): number {
    return this.observers.size;
  }

  getBoolPref(_name: string, _fallback: boolean): boolean {
    return this.value;
  }

  addObserver(_name: string, observer: HoverReloadPrefObserver): void {
    this.observers.add(observer);
  }

  removeObserver(_name: string, observer: HoverReloadPrefObserver): void {
    this.observers.delete(observer);
  }

  set(value: boolean): void {
    this.value = value;
    for (const observer of this.observers) {
      observer(null, "nsPref:changed", ENABLED_PREF);
    }
  }
}

class FakeClock implements HoverReloadClock {
  private nextHandle = 1;
  private readonly callbacks = new Map<HoverReloadTimerHandle, () => void>();
  private lastScheduledCallback: (() => void) | null = null;
  readonly scheduledDelays: number[] = [];

  get pendingCount(): number {
    return this.callbacks.size;
  }

  setTimeout(
    callback: () => void,
    delay: number,
  ): HoverReloadTimerHandle {
    const handle = this.nextHandle++;
    this.scheduledDelays.push(delay);
    this.lastScheduledCallback = callback;
    this.callbacks.set(handle, callback);
    return handle;
  }

  clearTimeout(handle: HoverReloadTimerHandle): void {
    this.callbacks.delete(handle);
  }

  runAll(): void {
    const callbacks = [...this.callbacks.values()];
    this.callbacks.clear();
    for (const callback of callbacks) {
      callback();
    }
  }

  runLastScheduledEvenIfCleared(): void {
    const callback = this.lastScheduledCallback;
    this.lastScheduledCallback = null;
    callback?.();
  }
}

class FakeMutationObserver implements HoverReloadMutationObserver {
  observed = false;
  disconnected = false;

  constructor(private readonly callback: MutationCallback) {}

  observe(_target: Node, _options?: MutationObserverInit): void {
    this.observed = true;
  }

  disconnect(): void {
    this.disconnected = true;
  }

  trigger(): void {
    this.callback([], this as unknown as MutationObserver);
  }
}

type FixtureBrowser = HoverReloadBrowser & {
  selectedTab: HoverReloadTab;
};

type Fixture = {
  browser: FixtureBrowser;
  clock: FakeClock;
  container: Element;
  mutationObservers: FakeMutationObserver[];
  options: HoverReloadControllerOptions;
  prefs: FakePrefs;
  reloads: HoverReloadTab[];
  tabs: HoverReloadTab[];
  unloadTarget: EventTarget;
  cleanup: () => void;
};

function createTab(container: Element): HoverReloadTab {
  const tab = document.createElement("div");
  tab.classList.add("tabbrowser-tab");

  const stack = document.createElement("div");
  stack.classList.add("tab-stack");
  const content = document.createElement("div");
  content.classList.add("tab-content");
  const label = document.createElement("span");
  label.classList.add("tab-label");
  label.textContent = "Fixture tab";
  const audio = document.createElement("button");
  audio.classList.add("tab-audio-button");
  const close = document.createElement("button");
  close.classList.add("tab-close-button");

  content.append(label, audio, close);
  stack.appendChild(content);
  tab.appendChild(stack);
  container.appendChild(tab);
  return tab;
}

function createFixture(enabled = false, tabCount = 2): Fixture {
  const container = document.createElement("div");
  browserRoot().appendChild(container);
  const tabs = Array.from({ length: tabCount }, () => createTab(container));
  const reloads: HoverReloadTab[] = [];
  const browser: FixtureBrowser = {
    tabs,
    tabContainer: container,
    selectedTab: tabs[0],
    reloadTab: (tab) => reloads.push(tab),
  };
  const prefs = new FakePrefs(enabled);
  const clock = new FakeClock();
  const mutationObservers: FakeMutationObserver[] = [];
  const unloadTarget = new EventTarget();
  const options: HoverReloadControllerOptions = {
    browser,
    document: document as HoverReloadDocument,
    prefs,
    clock,
    hoverDelayMs: 700,
    unloadTarget,
    label: "Localized reload label",
    mutationObserverFactory: (callback) => {
      const observer = new FakeMutationObserver(callback);
      mutationObservers.push(observer);
      return observer;
    },
  };

  const selectOnMouseDown = (event: Event) => {
    const target = event.target;
    if (!(target instanceof Element)) {
      return;
    }
    const tab = target.closest(".tabbrowser-tab");
    if (tab) {
      browser.selectedTab = tab;
    }
  };
  container.addEventListener("mousedown", selectOnMouseDown);

  return {
    browser,
    clock,
    container,
    mutationObservers,
    options,
    prefs,
    reloads,
    tabs,
    unloadTarget,
    cleanup: () => {
      container.removeEventListener("mousedown", selectOnMouseDown);
      container.remove();
      browserRoot().removeAttribute(ROOT_ATTR);
      for (const style of document.querySelectorAll(`[${STYLE_MARKER_ATTR}]`)) {
        style.remove();
      }
      for (const glyph of document.querySelectorAll(`.${GLYPH_CLASS}`)) {
        glyph.remove();
      }
    },
  };
}

function hover(tab: HoverReloadTab): void {
  tab.dispatchEvent(new CustomEvent("TabHoverStart", { bubbles: true }));
}

function leave(tab: HoverReloadTab): void {
  tab.dispatchEvent(new CustomEvent("TabHoverEnd", { bubbles: true }));
}

function glyphFor(tab: HoverReloadTab): Element | null {
  return tab.querySelector(`.${GLYPH_CLASS}`);
}

function testDefaultPreferenceIsOff(): void {
  const defaultBranch = Services.prefs.getDefaultBranch("");
  assertEquals(
    defaultBranch.getBoolPref(ENABLED_PREF, true),
    false,
    "hover reload should ship disabled on the default pref branch",
  );
}

function testDisabledStateHasNoTargetOrInteraction(): void {
  const fixture = createFixture(false);
  const controller = new HoverReloadController(fixture.options);
  try {
    controller.start();
    hover(fixture.tabs[1]);
    fixture.clock.runAll();

    assertEquals(fixture.clock.pendingCount, 0);
    assertEquals(document.querySelectorAll(`.${GLYPH_CLASS}`).length, 0);
    assertEquals(document.querySelectorAll(`[${STYLE_MARKER_ATTR}]`).length, 0);
    assert(!browserRoot().hasAttribute(ROOT_ATTR));
    assertEquals(fixture.reloads.length, 0);
  } finally {
    controller.destroy();
    fixture.cleanup();
  }
}

function testEnableDelayTargetAndSelectionPreservation(): void {
  const fixture = createFixture(true);
  const controller = new HoverReloadController(fixture.options);
  try {
    controller.start();
    hover(fixture.tabs[1]);
    assertEquals(glyphFor(fixture.tabs[1]), null, "target must wait for delay");
    assertEquals(fixture.clock.pendingCount, 1);
    assertEquals(
      fixture.clock.scheduledDelays.at(-1),
      700,
      "controller should schedule the documented 700 ms delay",
    );

    fixture.clock.runAll();
    const glyph = glyphFor(fixture.tabs[1]);
    assert(glyph !== null, "hovered tab should receive the reload target");
    assertEquals(
      glyphFor(fixture.tabs[0]),
      null,
      "other tab must stay unchanged",
    );
    assertEquals(glyph.getAttribute("aria-label"), "Localized reload label");
    assertEquals(glyph.getAttribute("tooltiptext"), "Localized reload label");
    assertEquals(
      glyph.nextElementSibling?.classList.contains("tab-close-button"),
      true,
      "reload target should occupy normal flow immediately before close",
    );

    controller.setLabel("Updated reload label");
    assertEquals(glyph.getAttribute("aria-label"), "Updated reload label");
    assertEquals(glyph.getAttribute("tooltiptext"), "Updated reload label");

    const mouseDown = new MouseEvent("mousedown", {
      bubbles: true,
      cancelable: true,
      button: 0,
    });
    glyph.dispatchEvent(mouseDown);
    glyph.dispatchEvent(
      new MouseEvent("click", { bubbles: true, cancelable: true, button: 0 }),
    );

    assert(mouseDown.defaultPrevented, "reload mousedown should be cancelled");
    assertEquals(
      fixture.browser.selectedTab,
      fixture.tabs[0],
      "reload activation must not select the target tab",
    );
    assertEquals(fixture.reloads.length, 1);
    assertEquals(fixture.reloads[0], fixture.tabs[1]);
  } finally {
    controller.destroy();
    fixture.cleanup();
  }
}

function testTimerCancellationPreventsLateTarget(): void {
  const fixture = createFixture(true);
  const controller = new HoverReloadController(fixture.options);
  try {
    controller.start();
    hover(fixture.tabs[1]);
    assertEquals(fixture.clock.pendingCount, 1);
    leave(fixture.tabs[1]);
    assertEquals(fixture.clock.pendingCount, 0);

    fixture.clock.runAll();
    assertEquals(glyphFor(fixture.tabs[1]), null);
    assert(!fixture.tabs[1].hasAttribute(STAMP_ATTR));
  } finally {
    controller.destroy();
    fixture.cleanup();
  }
}

function testRawMouseEventsDoNotOwnHoverState(): void {
  const fixture = createFixture(true);
  const controller = new HoverReloadController(fixture.options);
  try {
    controller.start();
    const content = fixture.tabs[1].querySelector(".tab-content");
    assert(content !== null, "fixture tab should have tab content");
    const close = fixture.tabs[1].querySelector(".tab-close-button");
    assert(close !== null, "fixture tab should have a close button");
    const audio = fixture.tabs[1].querySelector(".tab-audio-button");
    assert(audio !== null, "fixture tab should have an audio button");

    for (const target of [content, close, audio]) {
      target.dispatchEvent(new MouseEvent("mouseover", { bubbles: true }));
      target.dispatchEvent(
        new MouseEvent("mouseout", { bubbles: true, relatedTarget: null }),
      );
    }
    assertEquals(
      fixture.clock.pendingCount,
      0,
      "raw mouse events must not activate the hover timer",
    );

    hover(fixture.tabs[1]);
    assertEquals(fixture.clock.pendingCount, 1);
    content.dispatchEvent(new MouseEvent("mouseover", { bubbles: true }));
    content.dispatchEvent(
      new MouseEvent("mouseout", { bubbles: true, relatedTarget: null }),
    );
    assertEquals(
      fixture.clock.pendingCount,
      1,
      "raw mouse events must not duplicate or cancel the native hover timer",
    );
    leave(fixture.tabs[1]);
    assertEquals(fixture.clock.pendingCount, 0);
  } finally {
    controller.destroy();
    fixture.cleanup();
  }
}

function testBlockedControlsSuspendNativeHoverTarget(): void {
  const fixture = createFixture(true);
  const controller = new HoverReloadController(fixture.options);
  try {
    controller.start();
    const tab = fixture.tabs[1];
    const content = tab.querySelector(".tab-content");
    const close = tab.querySelector(".tab-close-button");
    const audio = tab.querySelector(".tab-audio-button");
    assert(content !== null, "fixture tab should have tab content");
    assert(close !== null, "fixture tab should have a close button");
    assert(audio !== null, "fixture tab should have an audio button");

    hover(tab);
    assertEquals(fixture.clock.pendingCount, 1);

    content.dispatchEvent(
      new MouseEvent("mouseout", { bubbles: true, relatedTarget: close }),
    );
    close.dispatchEvent(
      new MouseEvent("mouseover", { bubbles: true, relatedTarget: content }),
    );
    assertEquals(
      fixture.clock.pendingCount,
      0,
      "entering close must cancel the native hover timer",
    );
    fixture.clock.runLastScheduledEvenIfCleared();
    assertEquals(glyphFor(tab), null);

    close.dispatchEvent(
      new MouseEvent("mouseout", { bubbles: true, relatedTarget: content }),
    );
    content.dispatchEvent(
      new MouseEvent("mouseover", { bubbles: true, relatedTarget: close }),
    );
    assertEquals(
      fixture.clock.pendingCount,
      1,
      "returning to content must schedule exactly one timer",
    );
    assertEquals(fixture.clock.scheduledDelays.at(-1), 700);
    fixture.clock.runAll();
    const staleGlyph = glyphFor(tab);
    assert(staleGlyph !== null, "content hover should expose the target");

    staleGlyph.addEventListener(
      "click",
      () => {
        content.dispatchEvent(
          new MouseEvent("mouseout", { bubbles: true, relatedTarget: audio }),
        );
        audio.dispatchEvent(
          new MouseEvent("mouseover", {
            bubbles: true,
            relatedTarget: content,
          }),
        );
      },
      { capture: true, once: true },
    );
    staleGlyph.dispatchEvent(
      new MouseEvent("click", { bubbles: true, cancelable: true, button: 0 }),
    );
    assertEquals(glyphFor(tab), null, "entering audio must remove the target");
    assertEquals(fixture.clock.pendingCount, 0);
    assertEquals(
      fixture.reloads.length,
      0,
      "a blocked transition during activation must fail closed",
    );

    audio.dispatchEvent(
      new MouseEvent("mouseout", { bubbles: true, relatedTarget: content }),
    );
    content.dispatchEvent(
      new MouseEvent("mouseover", { bubbles: true, relatedTarget: audio }),
    );
    assertEquals(
      fixture.clock.pendingCount,
      1,
      "returning from audio must schedule exactly one timer",
    );
    leave(tab);
    assertEquals(fixture.clock.pendingCount, 0);
  } finally {
    controller.destroy();
    fixture.cleanup();
  }
}

function testLifecycleSignalsCancelAndResetPendingTarget(): void {
  const fixture = createFixture(true);
  const controller = new HoverReloadController(fixture.options);
  const cancellationSignals: Array<{ name: string; dispatch: () => void }> = [
    {
      name: "mouseleave",
      dispatch: () => fixture.container.dispatchEvent(new Event("mouseleave")),
    },
    {
      name: "TabSelect",
      dispatch: () => fixture.container.dispatchEvent(new Event("TabSelect")),
    },
    {
      name: "TabClose",
      dispatch: () => fixture.container.dispatchEvent(new Event("TabClose")),
    },
    {
      name: "dragstart",
      dispatch: () => fixture.container.dispatchEvent(new Event("dragstart")),
    },
    {
      name: "window blur",
      dispatch: () => fixture.unloadTarget.dispatchEvent(new Event("blur")),
    },
    {
      name: "visibility change",
      dispatch: () => document.dispatchEvent(new Event("visibilitychange")),
    },
  ];

  try {
    controller.start();
    for (const signal of cancellationSignals) {
      hover(fixture.tabs[1]);
      assertEquals(
        fixture.clock.pendingCount,
        1,
        `${signal.name} setup should own one timer`,
      );
      signal.dispatch();
      assertEquals(
        fixture.clock.pendingCount,
        0,
        `${signal.name} should cancel the timer`,
      );
      fixture.clock.runAll();
      assertEquals(
        glyphFor(fixture.tabs[1]),
        null,
        `${signal.name} must not leave a late target`,
      );
    }
  } finally {
    controller.destroy();
    fixture.cleanup();
  }
}

function testEligibilityChangesCancelPendingTarget(): void {
  const fixture = createFixture(true);
  const controller = new HoverReloadController(fixture.options);
  try {
    controller.start();
    hover(fixture.tabs[1]);
    assertEquals(fixture.clock.pendingCount, 1);

    fixture.tabs[1].setAttribute("soundplaying", "true");
    fixture.mutationObservers.at(-1)?.trigger();
    assertEquals(fixture.clock.pendingCount, 0);
    fixture.clock.runAll();
    assertEquals(glyphFor(fixture.tabs[1]), null);
  } finally {
    controller.destroy();
    fixture.cleanup();
  }
}

function testConflictingTabStatesNeverActivate(): void {
  const fixture = createFixture(true);
  const controller = new HoverReloadController(fixture.options);
  const blockedAttributes = [
    "activemedia-blocked",
    "attention",
    "closing",
    "muted",
    "pinned",
    "soundplaying",
    "usercontextid",
  ];
  try {
    controller.start();
    const tab = fixture.tabs[1];
    for (const attribute of blockedAttributes) {
      tab.setAttribute(attribute, "true");
      hover(tab);
      fixture.clock.runAll();
      assertEquals(
        glyphFor(tab),
        null,
        `${attribute} tabs must not expose reload target`,
      );
      leave(tab);
      tab.removeAttribute(attribute);
    }

    assert(!tab.hasAttribute("closing"));
    tab.closing = true;
    hover(tab);
    assertEquals(
      fixture.clock.pendingCount,
      0,
      "closing property alone must prevent timer ownership",
    );
    fixture.clock.runAll();
    assertEquals(
      glyphFor(tab),
      null,
      "closing property alone must block the reload target",
    );
    leave(tab);
    delete tab.closing;

    fixture.container.setAttribute("overflow", "true");
    hover(tab);
    fixture.clock.runAll();
    assertEquals(glyphFor(tab), null, "overflowing strip must not activate");
    fixture.container.removeAttribute("overflow");

    const content = tab.querySelector(".tab-content");
    assert(content !== null, "fixture tab should have tab content");
    content.setAttribute("attention", "true");
    hover(tab);
    fixture.clock.runAll();
    assertEquals(glyphFor(tab), null, "attention content must not activate");
    content.removeAttribute("attention");

    const group = document.createElement("tab-group");
    fixture.container.appendChild(group);
    group.appendChild(tab);
    hover(tab);
    fixture.clock.runAll();
    assertEquals(glyphFor(tab), null, "grouped tab must not activate");
    fixture.container.appendChild(tab);
    group.remove();
  } finally {
    controller.destroy();
    fixture.cleanup();
  }
}

function testKeyboardPolicyPreservesTabNavigation(): void {
  const fixture = createFixture(true);
  const controller = new HoverReloadController(fixture.options);
  try {
    controller.start();
    hover(fixture.tabs[1]);
    fixture.clock.runAll();
    const glyph = glyphFor(fixture.tabs[1]);
    assert(glyph !== null, "reload target should exist after delay");
    assertEquals(glyph.getAttribute("role"), "button");
    assertEquals(glyph.getAttribute("keyNav"), "false");
    assertEquals(glyph.getAttribute("tabindex"), "-1");
    assertEquals(glyph.getAttribute("aria-label"), "Localized reload label");

    const selectedBefore = fixture.browser.selectedTab;
    glyph.dispatchEvent(
      new KeyboardEvent("keydown", {
        bubbles: true,
        cancelable: true,
        key: "Enter",
      }),
    );
    assertEquals(
      fixture.reloads.length,
      0,
      "nested hover control must not capture tab keyboard activation",
    );
    assertEquals(fixture.browser.selectedTab, selectedBefore);
  } finally {
    controller.destroy();
    fixture.cleanup();
  }
}

function testDisableAndUnloadPerformFullCleanup(): void {
  const fixture = createFixture(true);
  const controller = new HoverReloadController(fixture.options);
  try {
    controller.start();
    hover(fixture.tabs[1]);
    fixture.clock.runAll();
    const staleGlyph = glyphFor(fixture.tabs[1]);
    assert(staleGlyph !== null, "target should exist before cleanup");

    fixture.prefs.set(false);
    assertEquals(glyphFor(fixture.tabs[1]), null);
    assert(!fixture.tabs[1].hasAttribute(STAMP_ATTR));
    assert(!browserRoot().hasAttribute(ROOT_ATTR));
    assertEquals(document.querySelectorAll(`[${STYLE_MARKER_ATTR}]`).length, 0);
    assert(fixture.mutationObservers.at(-1)?.disconnected === true);
    assertEquals(
      fixture.prefs.observerCount,
      1,
      "pref observer enables live re-entry",
    );

    staleGlyph.dispatchEvent(
      new MouseEvent("click", { bubbles: true, cancelable: true, button: 0 }),
    );
    assertEquals(fixture.reloads.length, 0, "detached target must be inert");

    fixture.prefs.set(true);
    assert(browserRoot().hasAttribute(ROOT_ATTR));
    fixture.unloadTarget.dispatchEvent(new Event("unload"));
    assertEquals(fixture.prefs.observerCount, 0);
    assert(!browserRoot().hasAttribute(ROOT_ATTR));
    assertEquals(document.querySelectorAll(`[${STYLE_MARKER_ATTR}]`).length, 0);
  } finally {
    controller.destroy();
    fixture.cleanup();
  }
}

function testHmrReplacementDoesNotDuplicateHandlersOrArtifacts(): void {
  const fixture = createFixture(true);
  const host = {};
  let current: HoverReloadController | null = null;
  try {
    installHoverReloadController(fixture.options, host);
    current = installHoverReloadController(fixture.options, host);

    assertEquals(fixture.prefs.observerCount, 1);
    assertEquals(document.querySelectorAll(`[${STYLE_MARKER_ATTR}]`).length, 1);
    assert(fixture.mutationObservers[0]?.disconnected === true);

    hover(fixture.tabs[1]);
    assertEquals(
      fixture.clock.pendingCount,
      1,
      "replacement should leave exactly one hover timer owner",
    );
    fixture.clock.runAll();
    assertEquals(fixture.tabs[1].querySelectorAll(`.${GLYPH_CLASS}`).length, 1);
    fixture.container.dispatchEvent(new Event("TabGrouped"));
    assertEquals(
      fixture.tabs[1].querySelectorAll(`.${GLYPH_CLASS}`).length,
      0,
      "group lifecycle event must remove an already-visible target",
    );

    hover(fixture.tabs[1]);
    fixture.clock.runAll();
    assertEquals(fixture.tabs[1].querySelectorAll(`.${GLYPH_CLASS}`).length, 1);
    fixture.mutationObservers.at(-1)?.trigger();
    assertEquals(
      fixture.tabs[1].querySelectorAll(`.${GLYPH_CLASS}`).length,
      1,
      "observer callback must not duplicate a visible target",
    );
    assertEquals(fixture.clock.pendingCount, 0);

    glyphFor(fixture.tabs[1])?.dispatchEvent(
      new MouseEvent("click", { bubbles: true, cancelable: true, button: 0 }),
    );
    assertEquals(fixture.reloads.length, 1, "replacement must reload once");
  } finally {
    if (current) {
      uninstallHoverReloadController(current, host);
    }
    fixture.cleanup();
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "default preference is off",
      fn: testDefaultPreferenceIsOff,
    },
    {
      name: "disabled state has no target or interaction",
      fn: testDisabledStateHasNoTargetOrInteraction,
    },
    {
      name: "enable delay targets tab without selecting it",
      fn: testEnableDelayTargetAndSelectionPreservation,
    },
    {
      name: "leaving before delay cancels target",
      fn: testTimerCancellationPreventsLateTarget,
    },
    {
      name: "raw mouse events do not own native hover state",
      fn: testRawMouseEventsDoNotOwnHoverState,
    },
    {
      name: "close and audio controls suspend native hover target",
      fn: testBlockedControlsSuspendNativeHoverTarget,
    },
    {
      name: "selection close blur and visibility signals cancel target",
      fn: testLifecycleSignalsCancelAndResetPendingTarget,
    },
    {
      name: "eligibility changes cancel pending target",
      fn: testEligibilityChangesCancelPendingTarget,
    },
    {
      name: "conflicting tab states never activate",
      fn: testConflictingTabStatesNeverActivate,
    },
    {
      name: "keyboard policy preserves native tab navigation",
      fn: testKeyboardPolicyPreservesTabNavigation,
    },
    {
      name: "pref disable and unload perform full cleanup",
      fn: testDisableAndUnloadPerformFullCleanup,
    },
    {
      name: "HMR replacement does not duplicate handlers or artifacts",
      fn: testHmrReplacementDoesNotDuplicateHandlersOrArtifacts,
    },
  ];

  await runTests("tabRefresh.test.ts", tests);
}
