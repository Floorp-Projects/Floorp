// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import IdleMemoryReclaim, { RECLAIM_REQUEST_TOPIC } from "../index.ts";
import {
  DEFAULT_SETTINGS,
  IDLE_MEMORY_RECLAIM_PREF,
  IDLE_MEMORY_RECLAIM_STATS_PREF,
} from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import type { ReclaimConstructorTestInstance } from "./types.ts";

function notifyPreferenceObserver(observer: nsIObserver | null): void {
  const subject = {} as nsISupports;
  if (typeof observer === "function") {
    observer(subject, "nsPref:changed", IDLE_MEMORY_RECLAIM_PREF);
  } else {
    observer?.observe(subject, "nsPref:changed", IDLE_MEMORY_RECLAIM_PREF);
  }
}

async function withServices(
  enabled: boolean,
  test: (
    environment: ReturnType<typeof serviceEnvironment>,
  ) => void | Promise<void>,
) {
  const environment = serviceEnvironment(enabled);
  const prototype = IdleMemoryReclaim
    .prototype as unknown as ReclaimConstructorTestInstance;
  const methods = {
    getPreferences: prototype.getPreferences,
    getObserverService: prototype.getObserverService,
    getIdleService: prototype.getIdleService,
    getTimers: prototype.getTimers,
  };
  Object.assign(prototype, environment.methods);
  try {
    await test(environment);
  } finally {
    try {
      environment.closeAll();
    } finally {
      Object.assign(prototype, methods);
    }
  }
}

function serviceEnvironment(enabled: boolean) {
  const prefs = new Map<string, string>([[
    IDLE_MEMORY_RECLAIM_PREF,
    JSON.stringify({ ...DEFAULT_SETTINGS, enabled, minResidentMB: 0 }),
  ]]);
  const prefObservers = new Set<nsIObserver>();
  const idleObservers = new Map<nsIObserver, number>();
  const requestObservers = new Set<nsIObserver>();
  const timers = new Map<number, () => void>();
  const targets: EventTarget[] = [];
  let nextTimer = 1;
  const preferences = {
    getStringPref: (name: string, fallback = "") => prefs.get(name) ?? fallback,
    setStringPref: (name: string, value: string) => prefs.set(name, value),
    addObserver: (name: string, observer: nsIObserver) => {
      assertEquals(
        name,
        IDLE_MEMORY_RECLAIM_PREF,
        "constructor only watches its settings",
      );
      prefObservers.add(observer);
    },
    removeObserver: (_name: string, observer: nsIObserver) =>
      prefObservers.delete(observer),
  };
  const idleService = {
    idleTime: 120_000,
    addIdleObserver: (observer: nsIObserver, seconds: number) =>
      idleObservers.set(observer, seconds),
    removeIdleObserver: (observer: nsIObserver, seconds: number) => {
      assertEquals(
        idleObservers.get(observer),
        seconds,
        "idle removal uses the original threshold",
      );
      idleObservers.delete(observer);
    },
  };
  const observerService = {
    addObserver: (observer: nsIObserver, topic: string) => {
      assertEquals(
        topic,
        RECLAIM_REQUEST_TOPIC,
        "constructor watches only its request topic",
      );
      requestObservers.add(observer);
    },
    removeObserver: (observer: nsIObserver, topic: string) => {
      assertEquals(
        topic,
        RECLAIM_REQUEST_TOPIC,
        "cleanup removes its request topic",
      );
      requestObservers.delete(observer);
    },
  };
  const timerService = {
    setInterval: (callback: () => void) => {
      const id = nextTimer++;
      timers.set(id, callback);
      return id;
    },
    clearInterval: (id: number) => timers.delete(id),
  };
  const notify = (value: boolean) => {
    prefs.set(
      IDLE_MEMORY_RECLAIM_PREF,
      JSON.stringify({ ...DEFAULT_SETTINGS, enabled: value, minResidentMB: 0 }),
    );
    for (const observer of [...prefObservers]) {
      notifyPreferenceObserver(observer);
    }
  };
  return {
    prefs,
    prefObservers,
    idleObservers,
    requestObservers,
    timers,
    notify,
    methods: {
      getPreferences: () => preferences as unknown as nsIPrefBranch,
      getObserverService: () =>
        observerService as unknown as nsIObserverService,
      getIdleService: () => idleService as unknown as nsIUserIdleService,
      getTimers: () =>
        timerService as unknown as Pick<
          Window,
          "setInterval" | "clearInterval"
        >,
    },
    construct: () => {
      const target = new EventTarget();
      targets.push(target);
      const instance = new IdleMemoryReclaim(
        target,
      ) as unknown as ReclaimConstructorTestInstance;
      return {
        instance,
        close: () => target.dispatchEvent(new Event("unload")),
      };
    },
    closeAll: () =>
      targets.forEach((target) => target.dispatchEvent(new Event("unload"))),
  };
}

async function testSavedOptInSurvivesConstruction() {
  await withServices(true, async (environment) => {
    const first = environment.construct();
    const state = first.instance;
    assertEquals(
      state.settings.enabled,
      true,
      "constructor must preserve saved consent",
    );
    assert(
      state.prefObserver !== null,
      "constructor must retain its pref observer",
    );
    assert(
      state.idleObserver !== null,
      "constructor must retain its idle observer",
    );
    assert(
      state.pollTimerId !== null,
      "constructor must retain its poll timer",
    );
    let calls = 0;
    state.takeSnapshot = () =>
      Promise.resolve({ residentBytes: 1, ghostWindows: 0 });
    state.runMinimizeMemoryUsage = () => {
      calls++;
      return Promise.resolve();
    };
    await state.reclaimIfNeeded(true);
    assertEquals(
      calls,
      1,
      "saved opt-in must permit reclaim without a pref toggle",
    );
    assertEquals(
      JSON.parse(environment.prefs.get(IDLE_MEMORY_RECLAIM_STATS_PREF) ?? "{}")
        .runCount,
      1,
      "the constructed instance records the completed run",
    );
    first.close();
    assertEquals(
      environment.prefObservers.size,
      0,
      "close removes the first pref observer",
    );
    assertEquals(
      environment.idleObservers.size,
      0,
      "close removes the first idle observer",
    );
    assertEquals(
      environment.requestObservers.size,
      0,
      "close removes the first request observer",
    );
    assertEquals(environment.timers.size, 0, "close clears the first timer");
    const restarted = environment.construct();
    assertEquals(
      restarted.instance.settings.enabled,
      true,
      "fresh construction reloads saved ON settings",
    );
    assertEquals(
      environment.prefObservers.size,
      1,
      "fresh construction installs one pref observer",
    );
    assertEquals(
      environment.requestObservers.size,
      1,
      "fresh construction installs one request observer",
    );
  });
}

async function testDefaultOffCloseCannotReactivate() {
  await withServices(false, (environment) => {
    const window = environment.construct();
    const pendingObserver = window.instance.prefObserver;
    assertEquals(
      environment.prefObservers.size,
      1,
      "OFF still installs one removable settings observer",
    );
    assertEquals(
      environment.requestObservers.size,
      0,
      "OFF installs no request observer",
    );
    window.close();
    window.close();
    assertEquals(
      window.instance.disposed,
      true,
      "window unload marks the instance disposed",
    );
    assertEquals(
      environment.prefObservers.size,
      0,
      "OFF window unload removes its settings observer",
    );
    environment.notify(true);
    notifyPreferenceObserver(pendingObserver);
    assertEquals(
      environment.idleObservers.size,
      0,
      "a closed instance cannot register idle again",
    );
    assertEquals(
      environment.requestObservers.size,
      0,
      "OFF to ON cannot revive a closed instance",
    );
    assertEquals(
      environment.timers.size,
      0,
      "a late pref callback cannot arm a closed-window timer",
    );
  });
}

async function testLiveToggleKeepsOneRegistration() {
  await withServices(false, (environment) => {
    const { instance } = environment.construct();
    for (let cycle = 0; cycle < 3; cycle++) {
      environment.notify(true);
      instance.init();
      assertEquals(
        environment.prefObservers.size,
        1,
        "repeat init cannot duplicate settings observers",
      );
      assertEquals(
        environment.idleObservers.size,
        1,
        "ON owns one idle observer",
      );
      assertEquals(
        environment.requestObservers.size,
        1,
        "ON owns one request observer",
      );
      assertEquals(environment.timers.size, 1, "ON owns one poll timer");
      environment.notify(false);
      assertEquals(
        environment.idleObservers.size,
        0,
        "OFF removes idle registration",
      );
      assertEquals(
        environment.requestObservers.size,
        0,
        "OFF removes request registration",
      );
      assertEquals(environment.timers.size, 0, "OFF clears polling");
    }
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("constructorLifecycle.test.ts", [
    {
      name: "saved opt-in survives real constructor and reconstruction",
      fn: testSavedOptInSurvivesConstruction,
    },
    {
      name: "default OFF window cannot reactivate after unload",
      fn: testDefaultOffCloseCannotReactivate,
    },
    {
      name: "live OFF ON toggles retain one registration",
      fn: testLiveToggleKeepsOneRegistration,
    },
  ]);
}
