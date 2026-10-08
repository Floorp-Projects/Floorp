// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { ContextMenuCatalogBuilder } from "../catalog.ts";
import { ContextMenuConfigStore } from "../config-store.ts";
import { ContextMenuController } from "../controller.ts";
import {
  ContextMenuRegistry,
  type ResolvedContextMenuSurface,
} from "../registry.ts";
import { CONTEXT_MENU_EXPERIMENT_ID, ContextMenuRuntime } from "../runtime.ts";
import type {
  ContextMenuExperiments,
  ContextMenuExperimentState,
  ContextMenuRuntimePreferences,
} from "../runtime-types.ts";
import { FLOORP_CONTEXT_HIDDEN_ATTRIBUTE } from "../style.ts";
import {
  CONTEXT_MENU_CONFIG_PREF,
  CONTEXT_MENU_ENABLED_PREF,
  type ContextMenuCatalogReporter,
  type ContextMenuCatalogSnapshot,
} from "../types.ts";

const POLICY_PREF = "floorp.experiments.participationPolicy";
const LAST_POLICY_PREF = "floorp.experiments.lastPolicy";
const POPUP_ID = "floorp-context-menu-flasco-test";
const SAVED_CONFIG = JSON.stringify({
  schemaVersion: 1,
  surfaces: {
    "flasco.test": {
      base: { root: { order: ["b", "a"], hidden: ["a"] } },
      profiles: {},
    },
  },
});

class Preferences implements ContextMenuRuntimePreferences {
  readonly strings = new Map([[POLICY_PREF, "default"], [
    LAST_POLICY_PREF,
    "default",
  ], [CONTEXT_MENU_CONFIG_PREF, SAVED_CONFIG]]);
  readonly observers = new Map<string, Set<nsIObserver>>();
  enabled = true;
  configReads = 0;
  getBoolPref(): boolean {
    return this.enabled;
  }
  getStringPref(name: string, fallback = ""): string {
    if (name === CONTEXT_MENU_CONFIG_PREF) this.configReads++;
    return this.strings.get(name) ?? fallback;
  }
  addObserver(name: string, observer: nsIObserver): void {
    const entries = this.observers.get(name) ?? new Set();
    entries.add(observer);
    this.observers.set(name, entries);
  }
  removeObserver(name: string, observer: nsIObserver): void {
    this.observers.get(name)?.delete(observer);
  }
  notify(name: string): void {
    for (const observer of this.observers.get(name) ?? []) {
      if (typeof observer === "function") {
        observer(Services.prefs, "nsPref:changed", name);
      } else observer.observe(Services.prefs, "nsPref:changed", name);
    }
  }
}

class Experiments implements ContextMenuExperiments {
  manifestAvailable = true;
  initializing = false;
  fail = false;
  entries: ContextMenuExperimentState[] = [{
    id: CONTEXT_MENU_EXPERIMENT_ID,
    isActive: true,
    currentVariantId: "enabled",
    enrollmentStatus: "enrolled",
  }];
  readonly listeners = new Set<() => void>();
  getAllExperiments(): ContextMenuExperimentState[] {
    if (this.fail) throw new Error("test availability failure");
    return this.entries.map((entry) => ({
      ...entry,
      isActive: entry.isActive &&
        (!entry.end || Date.now() < Date.parse(entry.end)) &&
        (!entry.start || Date.now() >= Date.parse(entry.start)),
    }));
  }
  subscribe(listener: () => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }
  ensureActiveAssignment(): boolean {
    return false;
  }
  notify(): void {
    for (const listener of this.listeners) listener();
  }
}

class Catalog extends ContextMenuCatalogBuilder {
  seeds = 0;
  records = 0;
  override seed(surface: ResolvedContextMenuSurface): void {
    this.seeds++;
    return super.seed(surface);
  }
  override record(
    surface: ResolvedContextMenuSurface,
  ): ContextMenuCatalogSnapshot {
    this.records++;
    return super.record(surface);
  }
}

function fixture() {
  const popup = document.createElement("menupopup");
  popup.id = POPUP_ID;
  const a = document.createElement("menuitem");
  a.id = "flasco-test-a";
  a.setAttribute("label", "Native A");
  const b = document.createElement("menuitem");
  b.id = "flasco-test-b";
  b.setAttribute("label", "Native B");
  popup.append(a, b);
  (document.body ?? document.documentElement).append(popup);
  const experiments = new Experiments();
  const preferences = new Preferences();
  const callbacks: Array<() => void> = [];
  const registry = new ContextMenuRegistry([{
    key: "flasco.test",
    label: "Test",
    documentURIs: [document.documentURI],
    popupSelectors: [`#${POPUP_ID}`],
    aliases: [{ key: "a", selectors: ["#flasco-test-a"] }, {
      key: "b",
      selectors: ["#flasco-test-b"],
    }],
    readonlySelectors: [],
    profiles: [{ key: "default", label: "Default" }],
    getProfileKey: () => "default",
  }]);
  const catalogs: Catalog[] = [];
  let creations = 0;
  let reports = 0;
  let removals = 0;
  let listenerAdds = 0;
  let listenerRemoves = 0;
  const reporter: ContextMenuCatalogReporter = {
    report: () => {
      reports++;
    },
    removeOwner: () => {
      removals++;
    },
  };
  const originalAdd = document.addEventListener.bind(document);
  const originalRemove = document.removeEventListener.bind(document);
  Object.defineProperty(document, "addEventListener", {
    configurable: true,
    value: (
      type: string,
      listener: EventListenerOrEventListenerObject,
      options?: boolean | AddEventListenerOptions,
    ) => {
      if (type.startsWith("popup") && options === true) listenerAdds++;
      originalAdd(type, listener, options);
    },
  });
  Object.defineProperty(document, "removeEventListener", {
    configurable: true,
    value: (
      type: string,
      listener: EventListenerOrEventListenerObject,
      options?: boolean | EventListenerOptions,
    ) => {
      if (type.startsWith("popup") && options === true) listenerRemoves++;
      originalRemove(type, listener, options);
    },
  });
  const runtime = new ContextMenuRuntime({
    window,
    experiments,
    preferences,
    createController: () => {
      creations++;
      const catalog = new Catalog(registry);
      catalogs.push(catalog);
      return new ContextMenuController({
        window,
        registry,
        configStore: new ContextMenuConfigStore(preferences),
        catalogBuilder: catalog,
        catalogReporter: reporter,
        ownerId: "flasco-test",
        scheduleMicrotask: (cb) => callbacks.push(cb),
        scheduleOpeningPass: (cb) => callbacks.push(cb),
      });
    },
  });
  const flush = () => {
    while (callbacks.length) callbacks.shift()?.();
  };
  const open = () => {
    popup.dispatchEvent(new Event("popupshowing", { bubbles: true }));
    flush();
  };
  return {
    runtime,
    experiments,
    preferences,
    popup,
    a,
    b,
    catalogs,
    flush,
    open,
    counts: () => ({
      creations,
      reports,
      removals,
      listenerAdds,
      listenerRemoves,
    }),
    cleanup: () => {
      runtime.destroy();
      Reflect.deleteProperty(document, "addEventListener");
      Reflect.deleteProperty(document, "removeEventListener");
      popup.remove();
    },
  };
}

function assertNative(f: ReturnType<typeof fixture>): void {
  assertEquals(f.popup.firstElementChild, f.a, "native order is restored");
  assert(
    !f.a.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE),
    "native visibility is restored",
  );
  assertEquals(
    f.preferences.strings.get(CONTEXT_MENU_CONFIG_PREF),
    SAVED_CONFIG,
    "saved layout is untouched",
  );
}

function testUnavailableStates(): void {
  const states: Array<(f: ReturnType<typeof fixture>) => void> = [
    (f) => {
      f.experiments.manifestAvailable = false;
    },
    (f) => {
      f.experiments.initializing = true;
    },
    (f) => {
      f.experiments.entries = [];
    },
    (f) => {
      f.experiments.entries[0].enrollmentStatus = "not_in_rollout";
      f.experiments.entries[0].currentVariantId = null;
    },
    (f) => {
      f.experiments.entries[0].enrollmentStatus = "disabled";
    },
    (f) => {
      f.experiments.entries[0].enrollmentStatus = "control";
      f.experiments.entries[0].currentVariantId = "control";
    },
    (f) => {
      f.experiments.entries[0].isActive = false;
    },
    (f) => {
      f.preferences.strings.set(POLICY_PREF, "never");
      f.experiments.entries[0].enrollmentStatus = "force_enrolled";
    },
    (f) => {
      f.preferences.enabled = false;
    },
    (f) => {
      f.experiments.fail = true;
    },
  ];
  for (const state of states) {
    const f = fixture();
    try {
      state(f);
      let nativeEvents = 0;
      f.popup.addEventListener("popupshowing", () => {
        nativeEvents++;
      });
      f.runtime.start();
      f.runtime.start();
      f.open();
      assertEquals(
        f.counts().creations,
        0,
        "unavailable states never construct a controller or catalog",
      );
      assertEquals(
        f.counts().listenerAdds,
        0,
        "unavailable states never register popup listeners",
      );
      assertEquals(
        f.counts().reports,
        0,
        "unavailable states never publish a catalog",
      );
      assertEquals(
        f.preferences.configReads,
        0,
        "unavailable states never load the saved configuration",
      );
      assertEquals(nativeEvents, 1, "native popup listeners still execute");
      assertNative(f);
    } finally {
      f.cleanup();
    }
  }
}

function testTransitions(): void {
  const f = fixture();
  try {
    f.experiments.manifestAvailable = false;
    f.runtime.start();
    f.experiments.manifestAvailable = true;
    f.experiments.notify();
    f.runtime.start();
    f.experiments.notify();
    assertEquals(
      f.counts().creations,
      1,
      "successful acquisition attaches exactly once",
    );
    assertEquals(
      f.counts().listenerAdds,
      4,
      "one controller owns four popup listeners",
    );
    assert(f.catalogs[0].seeds > 0, "enrolled startup seeds the catalog");
    f.open();
    assertEquals(f.popup.firstElementChild, f.b, "saved ordering is applied");
    assert(
      f.a.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE),
      "saved visibility is applied",
    );
    const reports = f.counts().reports;
    f.experiments.entries[0].enrollmentStatus = "disabled";
    f.experiments.notify();
    assertNative(f);
    assertEquals(
      f.counts().listenerRemoves,
      4,
      "disabling removes every popup listener",
    );
    assertEquals(
      f.counts().removals,
      1,
      "disabling releases the catalog owner",
    );
    f.open();
    assertEquals(
      f.counts().reports,
      reports,
      "disabled popups cannot record a catalog",
    );
    f.experiments.entries[0].enrollmentStatus = "force_enrolled";
    f.experiments.notify();
    f.open();
    assertEquals(
      f.counts().creations,
      2,
      "re-enrollment creates one fresh controller",
    );
    assert(
      f.a.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE),
      "re-enrollment reuses the saved layout",
    );
    f.preferences.enabled = false;
    f.preferences.notify(CONTEXT_MENU_ENABLED_PREF);
    assertNative(f);
    f.preferences.enabled = true;
    f.preferences.notify(CONTEXT_MENU_ENABLED_PREF);
    f.open();
    assertEquals(
      f.counts().creations,
      3,
      "the feature preference can re-enable without losing settings",
    );
    f.runtime.destroy();
    f.runtime.destroy();
    assertNative(f);
    assertEquals(
      f.experiments.listeners.size,
      0,
      "destroy unsubscribes from Flasco",
    );
    assertEquals(
      [...f.preferences.observers.values()].reduce(
        (sum, set) => sum + set.size,
        0,
      ),
      0,
      "destroy removes all preference observers",
    );
    assertEquals(
      f.counts().listenerRemoves,
      12,
      "every attached controller is released once",
    );
    f.experiments.notify();
    assertEquals(f.counts().creations, 3, "destroyed consumers cannot restart");
  } finally {
    f.cleanup();
  }
}

function testPolicyAndFailures(): void {
  const f = fixture();
  try {
    f.runtime.start();
    f.open();
    f.preferences.strings.set(POLICY_PREF, "never");
    f.preferences.notify(POLICY_PREF);
    assertNative(f);
    f.preferences.strings.set(POLICY_PREF, "always");
    f.preferences.notify(POLICY_PREF);
    assertEquals(
      f.counts().creations,
      1,
      "always waits for assignment recalculation",
    );
    f.experiments.initializing = true;
    f.experiments.notify();
    f.preferences.strings.set(LAST_POLICY_PREF, "always");
    f.experiments.initializing = false;
    f.experiments.notify();
    f.open();
    assertEquals(
      f.counts().creations,
      2,
      "always accepts the client's enabled assignment",
    );
    f.preferences.strings.set(POLICY_PREF, "default");
    f.preferences.notify(POLICY_PREF);
    assertNative(f);
    f.preferences.strings.set(LAST_POLICY_PREF, "default");
    f.experiments.entries[0].currentVariantId = "control";
    f.experiments.notify();
    assertEquals(
      f.counts().creations,
      2,
      "default respects the client's rollout result",
    );
    f.experiments.entries[0].currentVariantId = "enabled";
    f.experiments.notify();
    f.open();
    f.experiments.initializing = true;
    f.experiments.notify();
    assertNative(f);
    f.experiments.initializing = false;
    f.experiments.manifestAvailable = false;
    f.experiments.notify();
    assertEquals(
      f.counts().creations,
      3,
      "a failed fetch cannot revive a cached enabled assignment",
    );
    f.experiments.manifestAvailable = true;
    f.experiments.notify();
    f.open();
    f.experiments.fail = true;
    f.experiments.notify();
    assertNative(f);
    f.experiments.fail = false;
    f.experiments.entries = [];
    f.experiments.notify();
    assertEquals(
      f.counts().creations,
      4,
      "manifest removal cannot re-enable the feature",
    );
  } finally {
    f.cleanup();
  }
}

function testQueuedWorkIsCancelled(): void {
  const f = fixture();
  try {
    f.runtime.start();
    const reports = f.counts().reports;
    f.popup.dispatchEvent(new Event("popupshowing", { bubbles: true }));
    f.experiments.entries[0].isActive = false;
    f.experiments.notify();
    f.flush();
    assertEquals(
      f.catalogs[0].records,
      0,
      "queued work cannot build a catalog after disabling",
    );
    assertEquals(
      f.counts().reports,
      reports,
      "queued work cannot republish its removed owner",
    );
    assertNative(f);
  } finally {
    f.cleanup();
  }
}

async function testExpiry(): Promise<void> {
  const f = fixture();
  try {
    f.experiments.entries[0].end = new Date(Date.now() + 50).toISOString();
    f.runtime.start();
    f.open();
    await new Promise<void>((resolve) => globalThis.setTimeout(resolve, 80));
    assertNative(f);
    assertEquals(
      f.counts().removals,
      1,
      "expiry stops the controller without another UI action",
    );
  } finally {
    f.cleanup();
  }
}

function testSynchronousAssignmentTimerCleanup(): void {
  const experiments = new Experiments();
  const preferences = new Preferences();
  experiments.entries[0].currentVariantId = null;
  experiments.entries[0].end = new Date(Date.now() + 60000).toISOString();
  experiments.ensureActiveAssignment = () => {
    experiments.entries[0].currentVariantId = "enabled";
    // The real client publishes a recovered assignment synchronously.
    experiments.notify();
    return true;
  };
  const timers = new Map<number, () => void>();
  let nextTimer = 0;
  let attachments = 0;
  let destructions = 0;
  const timerWindow = {
    setTimeout(callback: () => void) {
      timers.set(++nextTimer, callback);
      return nextTimer;
    },
    clearTimeout(id: number) {
      timers.delete(id);
    },
  } as unknown as Window;
  const runtime = new ContextMenuRuntime({
    window: timerWindow,
    experiments,
    preferences,
    createController: () => ({
      attach: () => attachments++,
      destroy: () => destructions++,
    }),
  });
  try {
    runtime.start();
    assertEquals(attachments, 1, "assignment notification attaches once");
    assertEquals(
      timers.size,
      1,
      "assignment recovery keeps one boundary timer",
    );
    experiments.notify();
    assertEquals(timers.size, 1, "repeated notification replaces the timer");
    runtime.destroy();
    assertEquals(timers.size, 0, "destroy cancels every owned boundary timer");
    assertEquals(destructions, 1, "destroy releases the controller once");
    experiments.notify();
    assertEquals(timers.size, 0, "destroyed runtime cannot schedule more work");
  } finally {
    runtime.destroy();
    timers.clear();
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name:
        "unavailable Flasco states leave native menus and never initialize catalogs or popup listeners",
      fn: testUnavailableStates,
    },
    {
      name:
        "participation and feature preference transitions preserve settings and release listeners",
      fn: testTransitions,
    },
    {
      name:
        "always/default/never, pending and failed fetches, and manifest removal",
      fn: testPolicyAndFailures,
    },
    {
      name: "disabling cancels queued popup catalog work",
      fn: testQueuedWorkIsCancelled,
    },
    { name: "experiment expiry releases the customizer", fn: testExpiry },
    {
      name:
        "synchronous assignment recovery retains one cancellable boundary timer",
      fn: testSynchronousAssignmentTimerCleanup,
    },
  ];
  await runTests("contextMenuFlasco.test.ts", tests);
}
