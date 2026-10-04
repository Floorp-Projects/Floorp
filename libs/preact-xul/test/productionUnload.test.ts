// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { signal } from "@preact/signals";
import type { ViteHotContext } from "vite/types/hot.js";
import { addDisposer, createRootHMR, rootEffect } from "../lifetime.ts";
import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
} from "../../../browser-features/chrome/test/utils/test_harness.ts";

declare const Services: {
  prefs: {
    addObserver(name: string, observer: { observe(): void }): void;
    removeObserver(name: string, observer: { observe(): void }): void;
    setBoolPref(name: string, value: boolean): void;
    clearUserPref(name: string): void;
  };
};

/**
 * Exercise real EventTarget dispatch without unloading the shared test browser.
 * Only listeners registered during synchronous test setup are redirected; the
 * browser's existing unload handlers and all other event types are untouched.
 */
function withIsolatedUnload(
  run: (
    target: EventTarget,
    active: Set<EventListenerOrEventListenerObject>,
  ) => void,
): void {
  const view = document.defaultView;
  assert(view, "test document has a window");
  const events: EventTarget = view;
  const add = events.addEventListener;
  const remove = events.removeEventListener;
  const addDescriptor = Object.getOwnPropertyDescriptor(
    view,
    "addEventListener",
  );
  const removeDescriptor = Object.getOwnPropertyDescriptor(
    view,
    "removeEventListener",
  );
  const target = new EventTarget();
  const active = new Set<EventListenerOrEventListenerObject>();
  events.addEventListener = (type, callback, options) => {
    if (type === "unload" && callback) {
      active.add(callback);
      target.addEventListener(type, callback, options);
    } else Reflect.apply(add, events, [type, callback, options]);
  };
  events.removeEventListener = (type, callback, options) => {
    if (type === "unload" && callback) {
      active.delete(callback);
      target.removeEventListener(type, callback, options);
    } else Reflect.apply(remove, events, [type, callback, options]);
  };
  try {
    run(target, active);
  } finally {
    if (addDescriptor) {
      Object.defineProperty(view, "addEventListener", addDescriptor);
    } else Reflect.deleteProperty(view, "addEventListener");
    if (removeDescriptor) {
      Object.defineProperty(view, "removeEventListener", removeDescriptor);
    } else Reflect.deleteProperty(view, "removeEventListener");
  }
}

function testProductionUnloadStopsExternalObservers(): void {
  withIsolatedUnload((target, active) => {
    const pref =
      `floorp.tests.preactProductionUnload.${Date.now()}.${Math.random()}`;
    const source = signal(0);
    let effectRuns = 0;
    let observations = 0;
    let cleanups = 0;
    let dispose = () => {};
    const observer = {
      observe() {
        observations++;
      },
    };
    Services.prefs.setBoolPref(pref, false);
    try {
      createRootHMR((cleanup) => {
        dispose = cleanup;
        rootEffect(() => {
          source.value;
          effectRuns++;
        });
        Services.prefs.addObserver(pref, observer);
        addDisposer(() => {
          Services.prefs.removeObserver(pref, observer);
          cleanups++;
        });
      });
      source.value++;
      Services.prefs.setBoolPref(pref, true);
      assertEquals(effectRuns, 2, "production root runs before unload");
      assertEquals(
        observations,
        1,
        "real prefs service notifies before unload",
      );
      target.dispatchEvent(new Event("unload"));
      source.value++;
      Services.prefs.setBoolPref(pref, false);
      assertEquals(
        effectRuns,
        2,
        "window unload stops production effect subscriptions",
      );
      assertEquals(
        observations,
        1,
        "window unload removes service-owned prefs observers",
      );
      assertEquals(cleanups, 1, "production root cleanup runs once");
      assertEquals(
        active.size,
        0,
        "disposed root no longer retains a window listener",
      );
      dispose();
      assertEquals(cleanups, 1, "manual cleanup after unload is idempotent");
    } finally {
      dispose();
      Services.prefs.clearUserPref(pref);
    }
  });
}

function testHMRDisposalDetachesUnloadListener(): void {
  withIsolatedUnload((target, active) => {
    let onHMR = () => {};
    let cleanups = 0;
    const hot = {
      data: {},
      dispose(callback: () => void) {
        onHMR = callback;
      },
    } as unknown as ViteHotContext;
    createRootHMR(() => addDisposer(() => cleanups++), hot);
    try {
      assertEquals(
        active.size,
        1,
        "development root also belongs to its window",
      );
      onHMR();
      assertEquals(active.size, 0, "HMR cleanup detaches the unload listener");
      target.dispatchEvent(new Event("unload"));
      assertEquals(
        cleanups,
        1,
        "HMR followed by unload does not clean up twice",
      );
    } finally {
      onHMR();
    }
  });
}

function testFailedSetupDetachesUnloadListener(): void {
  withIsolatedUnload((_target, active) => {
    let cleanups = 0;
    assertThrows(() =>
      createRootHMR(() => {
        addDisposer(() => cleanups++);
        throw new Error("expected failed root setup");
      }), "root setup error propagates");
    assertEquals(cleanups, 1, "failed setup still releases resources");
    assertEquals(
      active.size,
      0,
      "failed setup does not leave an unload listener",
    );
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("preact-xul/productionUnload.test.ts", [
    {
      name: "production unload releases real prefs observers and effects",
      fn: testProductionUnloadStopsExternalObservers,
    },
    {
      name: "HMR cleanup removes window lifetime listener",
      fn: testHMRDisposalDetachesUnloadListener,
    },
    {
      name: "failed initialization removes window lifetime listener",
      fn: testFailedSetupDetachesUnloadListener,
    },
  ]);
}
