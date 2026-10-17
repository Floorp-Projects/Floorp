// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createRootHMR, disposeRoot } from "@nora/solid-xul";
import {
  createEffect,
  createRoot,
  createSignal,
  getOwner,
  onCleanup,
} from "solid-js";
import type { ViteHotContext } from "vite/types/hot";
import TabStacks, { ENABLED_PREF } from "../index.ts";
import { getGBrowser, STACK_ATTR, syncActiveGroup } from "../stack-bar.tsx";
import { initializeTabStacksWhenReady } from "../startup.ts";
import type { TabStacksChrome, TabStacksStartupWindow } from "../types.ts";
import type {
  SessionStackWindow,
  StartupDeferred,
  StartupLogs,
  StartupMount,
} from "./startup-test-types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

function deferred(): StartupDeferred {
  let resolve: StartupDeferred["resolve"] = () => {};
  let reject: StartupDeferred["reject"] = () => {};
  const promise = new Promise<unknown>((done, fail) => {
    resolve = done;
    reject = fail;
  });
  return { promise, resolve, reject };
}

async function settle(): Promise<void> {
  for (let i = 0; i < 4; i++) await Promise.resolve();
}

function chrome(): TabStacksChrome {
  const gb = getGBrowser();
  const head = document.head;
  const toolbox = document.getElementById("navigator-toolbox");
  const navBar = document.getElementById("nav-bar");
  assert(gb && head && toolbox && navBar, "test window has browser chrome");
  return { gb, head, toolbox, navBar };
}

function logs() {
  const messages: StartupLogs = { info: [], warn: [], error: [] };
  return {
    messages,
    logger: {
      info: (...args: unknown[]) => messages.info.push(args),
      warn: (...args: unknown[]) => messages.warn.push(args),
      error: (...args: unknown[]) => messages.error.push(args),
    },
  };
}

function start(
  win: TabStacksStartupWindow,
  getChrome: () => TabStacksChrome | null,
  initialize: StartupMount,
  hot?: ViteHotContext,
) {
  const output = logs();
  const dispose = createRootHMR(
    (cleanup) => {
      initializeTabStacksWhenReady({
        window: win,
        getChrome,
        initialize,
        logger: output.logger,
      });
      return cleanup;
    },
    hot,
    win,
  );
  return { dispose, messages: output.messages };
}

async function testReadyStartupOwnsReactiveResources(): Promise<void> {
  const win = Object.assign(new EventTarget(), { closed: false });
  let mounted = 0;
  let cleanups = 0;
  let effects = 0;
  const [value, setValue] = createSignal(0);
  const fixture = start(win, chrome, () => {
    assert(getOwner(), "synchronous initialization has a Solid owner");
    mounted++;
    createEffect(() => {
      value();
      effects++;
    });
    onCleanup(() => cleanups++);
  });
  try {
    assertEquals(mounted, 1, "ready chrome initializes synchronously");
    await settle();
    setValue(1);
    assertEquals(effects, 2, "initialization effect remains reactive");
    win.dispatchEvent(new Event("unload"));
    setValue(2);
    fixture.dispose();
    assertEquals(cleanups, 1, "unload and explicit disposal clean once");
    assertEquals(
      effects,
      2,
      "disposed root has no live reactive subscriptions",
    );
  } finally {
    fixture.dispose();
  }
}

async function testIndependentRestoredWindowOrdering(): Promise<void> {
  // Simulates the ordering of normal and crash recovery startup. Native
  // SessionStore restoration is exercised separately in tabStacksSessionRestore.
  for (const order of [[1, 0, 2], [2, 1, 0]]) {
    const pending = [deferred(), deferred(), deferred()];
    const ready = [false, false, false];
    const mounted = [0, 0, 0];
    const disposed = [0, 0, 0];
    const fixtures = pending.map((item, index) =>
      start(
        Object.assign(new EventTarget(), {
          closed: false,
          delayedStartupPromise: item.promise,
        }),
        () => ready[index] ? chrome() : null,
        () => {
          mounted[index]++;
          onCleanup(() => disposed[index]++);
        },
      )
    );
    try {
      for (const index of order) {
        ready[index] = true;
        pending[index].resolve();
        await settle();
        for (let i = 0; i < pending.length; i++) {
          assertEquals(
            mounted[i],
            ready[i] ? 1 : 0,
            "each window uses its own readiness",
          );
        }
      }
      fixtures[order[0]].dispose();
      assertEquals(
        disposed[order[0]],
        1,
        "first restored window can close independently",
      );
      assertEquals(disposed[order[1]], 0, "other window's root survives");
      for (const item of pending) item.resolve();
      await settle();
      assertEquals(
        mounted.join(","),
        "1,1,1",
        "settled startup cannot mount twice",
      );
    } finally {
      for (const fixture of fixtures) fixture.dispose();
    }
    assertEquals(
      disposed.join(","),
      "1,1,1",
      "every window releases its own resources",
    );
  }
}

async function testDeferredMountRetainsOwner(): Promise<void> {
  const pending = deferred();
  const win = Object.assign(new EventTarget(), {
    closed: false,
    gBrowserInit: { delayedStartupPromise: pending.promise },
  });
  const [value, setValue] = createSignal(0);
  let ready: TabStacksChrome | null = null;
  let mountedChrome: TabStacksChrome | undefined;
  let effects = 0;
  let cleanups = 0;
  const fixture = start(win, () => ready, (current) => {
    assert(getOwner(), "promise continuation creates a Solid root");
    mountedChrome = current;
    createEffect(() => {
      value();
      effects++;
    });
    onCleanup(() => cleanups++);
  });
  try {
    ready = { ...chrome() };
    pending.resolve();
    await settle();
    assertEquals(mountedChrome, ready, "retry reads fresh chrome references");
    assertEquals(effects, 1, "deferred root runs its reactive effect");
    setValue(1);
    assertEquals(effects, 2, "deferred root supports updates");
    fixture.dispose();
    setValue(2);
    assertEquals(effects, 2, "parent disposal tears down deferred effect");
    assertEquals(cleanups, 1, "parent owns deferred cleanup");
  } finally {
    fixture.dispose();
    pending.resolve();
  }
}

async function testPendingDisposalCannotReviveWindow(): Promise<void> {
  for (const reason of ["manual", "unload", "hmr", "closed"]) {
    const pending = deferred();
    const win = Object.assign(new EventTarget(), {
      closed: false,
      delayedStartupPromise: pending.promise,
    });
    let ready = false;
    let mounts = 0;
    const hot = {
      data: { __solidXulExternalDisposeOwner: true },
    } as unknown as ViteHotContext;
    const fixture = start(
      win,
      () => ready ? chrome() : null,
      () => mounts++,
      hot,
    );
    try {
      if (reason === "manual") fixture.dispose();
      if (reason === "unload") win.dispatchEvent(new Event("unload"));
      if (reason === "hmr") disposeRoot(hot);
      if (reason === "closed") win.closed = true;
      ready = true;
      pending.resolve();
      await settle();
      assertEquals(mounts, 0, `${reason} prevents late initialization`);
    } finally {
      fixture.dispose();
      disposeRoot(hot);
      pending.resolve();
    }
  }
}

async function testRejectedAndIncompleteStartup(): Promise<void> {
  for (const rejected of [true, false]) {
    const pending = deferred();
    let mounts = 0;
    let reads = 0;
    const fixture = start(
      Object.assign(new EventTarget(), {
        closed: false,
        delayedStartupPromise: pending.promise,
      }),
      () => {
        reads++;
        return null;
      },
      () => mounts++,
    );
    try {
      if (rejected) pending.reject(new Error("expected startup rejection"));
      else pending.resolve();
      await settle();
      assertEquals(mounts, 0, "unready chrome is never initialized");
      assertEquals(reads, rejected ? 1 : 2, "startup retry is bounded");
      assertEquals(
        fixture.messages.error.length,
        rejected ? 1 : 0,
        "rejection is caught and reported",
      );
      assertEquals(
        fixture.messages.warn.length,
        rejected ? 0 : 1,
        "missing chrome after startup is reported once",
      );
    } finally {
      fixture.dispose();
    }
  }
}

async function testDeferredFailureDisposesPartialMount(): Promise<void> {
  const pending = deferred();
  let ready = false;
  let cleanups = 0;
  const failure = new Error("expected mount failure");
  const fixture = start(
    Object.assign(new EventTarget(), {
      closed: false,
      delayedStartupPromise: pending.promise,
    }),
    () => ready ? chrome() : null,
    () => {
      onCleanup(() => cleanups++);
      throw failure;
    },
  );
  try {
    ready = true;
    pending.resolve();
    await settle();
    assertEquals(
      cleanups,
      1,
      "failed deferred mount releases partial resources immediately",
    );
    assertEquals(
      fixture.messages.error.length,
      1,
      "failure is caught without an unhandled rejection",
    );
    assertEquals(
      fixture.messages.error[0][1],
      failure,
      "diagnostic retains original failure",
    );
    fixture.dispose();
    assertEquals(cleanups, 1, "later parent cleanup is idempotent");
  } finally {
    fixture.dispose();
  }
}

function observerCount(): number {
  let count = 0;
  const observers = Services.obs.enumerateObservers(
    "browser-delayed-startup-finished",
  );
  while (observers.hasMoreElements()) {
    observers.getNext();
    count++;
  }
  return count;
}

function testObserverFallbackIsOwned(): void {
  const baseline = observerCount();
  // Count the real registrations without broadcasting synthetic window startup
  // notifications to unrelated Firefox consumers or mutating native services.
  for (const reason of ["manual", "unload", "hmr"]) {
    const target = Object.assign(new EventTarget(), { closed: false });
    const hot = {
      data: { __solidXulExternalDisposeOwner: true },
    } as unknown as ViteHotContext;
    let mounts = 0;
    const fixture = start(target, () => null, () => mounts++, hot);
    try {
      assertEquals(
        observerCount(),
        baseline + 1,
        "fallback owns one native observer",
      );
      if (reason === "manual") fixture.dispose();
      if (reason === "unload") target.dispatchEvent(new Event("unload"));
      if (reason === "hmr") disposeRoot(hot);
      assertEquals(
        observerCount(),
        baseline,
        `${reason} removes the pending startup observer`,
      );
      assertEquals(mounts, 0, "canceled observer cannot install a partial UI");
    } finally {
      fixture.dispose();
      disposeRoot(hot);
    }
  }

  let reads = 0;
  let cleanups = 0;
  const fixture = start(
    Object.assign(new EventTarget(), {
      closed: false,
      gBrowserInit: { delayedStartupFinished: true },
    }),
    () => ++reads === 1 ? null : chrome(),
    () => onCleanup(() => cleanups++),
  );
  try {
    assertEquals(
      reads,
      2,
      "already-finished startup retries fresh prerequisites immediately",
    );
    assertEquals(
      observerCount(),
      baseline,
      "already-finished startup registers no observer",
    );
    fixture.dispose();
    assertEquals(
      cleanups,
      1,
      "already-finished fallback root is owned by its parent",
    );
  } finally {
    fixture.dispose();
  }
}

async function testFeatureGuardAndDisableBeforeRetry(): Promise<void> {
  const hadPref = Services.prefs.prefHasUserValue(ENABLED_PREF);
  const pref = Services.prefs.getBoolPref(ENABLED_PREF, false);
  const originalGet = document.getElementById;
  const savedPromise = Object.getOwnPropertyDescriptor(
    window,
    "delayedStartupPromise",
  );
  let mounts = 0;
  let cleanups = 0;
  let dispose: (() => void) | undefined;
  try {
    for (const disableBeforeRetry of [false, true]) {
      const startup = deferred();
      const feature = Object.create(TabStacks.prototype) as TabStacks;
      feature.logger = console.createInstance({
        prefix: "tab-stacks-startup-test",
      });
      // Spy on the feature's mount boundary; do not install another real stack
      // UI over the loader-owned instance in the test browser.
      Object.defineProperty(feature, "initialize", {
        value: () => {
          mounts++;
          onCleanup(() => cleanups++);
        },
      });
      Services.prefs.setBoolPref(ENABLED_PREF, true);
      Object.defineProperty(window, "delayedStartupPromise", {
        configurable: savedPromise?.configurable ?? true,
        value: startup.promise,
      });
      document.getElementById = function (id: string) {
        return id === "nav-bar" ? null : originalGet.call(this, id);
      };
      createRoot((cleanup) => {
        dispose = cleanup;
        feature.init();
        feature.init();
      });
      document.getElementById = originalGet;
      if (disableBeforeRetry) Services.prefs.setBoolPref(ENABLED_PREF, false);
      startup.resolve();
      await settle();
      assertEquals(
        mounts,
        1,
        "one mount despite repeat init and a disabled pending retry",
      );
      feature.init();
      dispose?.();
      assertEquals(cleanups, 1, "only the successful mount owns resources");
    }
  } finally {
    document.getElementById = originalGet;
    dispose?.();
    if (savedPromise) {
      Object.defineProperty(window, "delayedStartupPromise", savedPromise);
    } else {delete (window as unknown as { delayedStartupPromise?: unknown })
        .delayedStartupPromise;}
    if (hadPref) Services.prefs.setBoolPref(ENABLED_PREF, pref);
    else Services.prefs.clearUserPref(ENABLED_PREF);
  }
}

async function testMissingChromeRetriesRealPresentation(): Promise<void> {
  const hadPref = Services.prefs.prefHasUserValue(ENABLED_PREF);
  const pref = Services.prefs.getBoolPref(ENABLED_PREF, false);
  const originalGet = document.getElementById;
  const savedPromise = Object.getOwnPropertyDescriptor(
    window,
    "delayedStartupPromise",
  );
  const pending = deferred();
  const gb = (window as unknown as SessionStackWindow).gBrowser;
  const nativeHead = document.head;
  const nativeToolbox = document.getElementById("navigator-toolbox");
  const nativeNavBar = document.getElementById("nav-bar");
  const nativeStyles = [...document.querySelectorAll("style")].map((node) => ({
    node,
    parent: node.parentNode,
  }));
  const originalTab = gb.selectedTab;
  const tabs = Array.from({ length: 2 }, () =>
    gb.addTab("about:blank", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      inBackground: true,
    }));
  let dispose: (() => void) | undefined;
  try {
    assertEquals(
      document.querySelectorAll("#floorp-stack-kind-menu").length,
      0,
      "test profile has no existing Tab Stacks instance",
    );
    Services.prefs.setBoolPref(ENABLED_PREF, true);
    const group = gb.addTabGroup(tabs, { label: "Delayed chrome regression" });
    gb.selectedTab = tabs[0];
    const feature = Object.create(TabStacks.prototype) as TabStacks;
    feature.logger = console.createInstance({
      prefix: "tab-stacks-delayed-chrome-test",
    });
    Object.defineProperty(window, "delayedStartupPromise", {
      configurable: savedPromise?.configurable ?? true,
      value: pending.promise,
    });
    document.getElementById = function (id: string) {
      return id === "navigator-toolbox" ? null : originalGet.call(this, id);
    };
    createRoot((cleanup) => {
      dispose = cleanup;
      feature.init();
      feature.init();
    });
    document.getElementById = originalGet;
    assertEquals(
      document.querySelectorAll("#floorp-stack-kind-menu").length,
      0,
      "unready chrome has no partial installation",
    );
    pending.resolve();
    await settle();
    await new Promise<void>((resolve) =>
      requestAnimationFrame(() => resolve())
    );
    assert(
      group.hasAttribute(STACK_ATTR),
      "retry decorates the restored native group as a stack",
    );
    assertEquals(
      document.querySelectorAll("#floorp-stack-items .floorp-stack-tab").length,
      2,
      "retry renders every restored member in a second row",
    );
    const mount = document.getElementById("floorp-stack-mount");
    assert(mount, "deferred presentation has an explicitly owned mount");
    assertEquals(
      mount.parentNode,
      nativeToolbox,
      "owned mount stays inside the native toolbox",
    );
    assertEquals(
      mount.nextElementSibling,
      nativeNavBar,
      "owned mount preserves the row before the native address bar",
    );
    const bar = mount.querySelector("#floorp-stack-bar");
    assert(
      bar && bar.getBoundingClientRect().height > 0,
      "the second row has visible height through the mount",
    );
    feature.init();
    assertEquals(
      document.querySelectorAll("#floorp-stack-bar").length,
      1,
      "repeat initialization cannot duplicate the real stack bar",
    );
    assertEquals(
      document.querySelectorAll("#floorp-stack-kind-menu").length,
      1,
      "repeat initialization cannot duplicate the real menu",
    );
    dispose?.();
    assertEquals(
      document.querySelectorAll("#floorp-stack-bar").length,
      0,
      "parent disposal removes the deferred second row",
    );
    assertEquals(
      document.querySelectorAll("#floorp-stack-kind-menu").length,
      0,
      "parent disposal removes the deferred menu",
    );
    assertEquals(
      document.querySelectorAll("#floorp-stack-mount").length,
      0,
      "parent disposal removes the owned mount",
    );
    assertEquals(
      document.head,
      nativeHead,
      "cleanup preserves the native document head",
    );
    assertEquals(
      document.getElementById("navigator-toolbox"),
      nativeToolbox,
      "cleanup preserves the native toolbox",
    );
    assertEquals(
      document.getElementById("nav-bar"),
      nativeNavBar,
      "cleanup preserves the native address bar",
    );
    for (const { node, parent } of nativeStyles) {
      assert(
        node.isConnected && node.parentNode === parent,
        "cleanup preserves pre-existing style nodes",
      );
    }
  } finally {
    document.getElementById = originalGet;
    dispose?.();
    pending.resolve();
    if (savedPromise) {
      Object.defineProperty(window, "delayedStartupPromise", savedPromise);
    } else {delete (window as unknown as { delayedStartupPromise?: unknown })
        .delayedStartupPromise;}
    for (const tab of tabs) {
      if (tab.isConnected) gb.removeTab(tab, { animate: false });
    }
    if (originalTab.isConnected) gb.selectedTab = originalTab;
    syncActiveGroup();
    if (hadPref) Services.prefs.setBoolPref(ENABLED_PREF, pref);
    else Services.prefs.clearUserPref(ENABLED_PREF);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("tabStacksStartup.test.ts", [
    {
      name: "normal ready startup owns reactive resources until unload",
      fn: testReadyStartupOwnsReactiveResources,
    },
    {
      name: "normal and simulated crash restore window ordering is independent",
      fn: testIndependentRestoredWindowOrdering,
    },
    {
      name:
        "delayed startup uses fresh chrome and a parent-owned reactive root",
      fn: testDeferredMountRetainsOwner,
    },
    {
      name:
        "manual disposal, unload, HMR, and closed windows cancel pending startup",
      fn: testPendingDisposalCannotReviveWindow,
    },
    {
      name:
        "rejected startup and missing chrome terminate with bounded diagnostics",
      fn: testRejectedAndIncompleteStartup,
    },
    {
      name: "failed deferred initialization disposes partial resources",
      fn: testDeferredFailureDisposesPartialMount,
    },
    {
      name:
        "observer fallback releases its observer and handles already-finished startup",
      fn: testObserverFallbackIsOwned,
    },
    {
      name:
        "repeat feature init and preference changes cannot duplicate or revive startup",
      fn: testFeatureGuardAndDisableBeforeRetry,
    },
    {
      name:
        "missing startup chrome retries and cleans the real stack presentation",
      fn: testMissingChromeRetriesRealPresentation,
    },
  ]);
}
