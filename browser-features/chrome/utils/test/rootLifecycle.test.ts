// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createRootHMR, disposeRoot } from "@nora/solid-xul";
import { onCleanup } from "solid-js";
import type { ViteHotContext } from "vite/types/hot";
import { assertEquals, runTests } from "../../test/utils/test_harness.ts";

class LifetimeTarget extends EventTarget {
  readonly listeners = new Set<EventListenerOrEventListenerObject>();

  override addEventListener(
    type: string,
    listener: EventListenerOrEventListenerObject | null,
    options?: AddEventListenerOptions | boolean,
  ): void {
    if (type === "unload" && listener) this.listeners.add(listener);
    super.addEventListener(type, listener, options);
  }

  override removeEventListener(
    type: string,
    listener: EventListenerOrEventListenerObject | null,
    options?: EventListenerOptions | boolean,
  ): void {
    if (type === "unload" && listener) this.listeners.delete(listener);
    super.removeEventListener(type, listener, options);
  }

  unload(): void {
    this.dispatchEvent(new Event("unload"));
  }
}

function hotContext() {
  let callback: Parameters<ViteHotContext["dispose"]>[0] | undefined;
  let registrations = 0;
  const hot = {
    data: {},
    dispose(handler: Parameters<ViteHotContext["dispose"]>[0]) {
      registrations++;
      callback = handler;
    },
  } as ViteHotContext;
  return {
    hot,
    trigger: () => callback?.(hot.data),
    registrations: () => registrations,
  };
}

function testUnloadAndReentrantCleanup() {
  const target = new LifetimeTarget();
  let cleanups = 0;
  const dispose = createRootHMR(
    (dispose) => {
      onCleanup(() => {
        cleanups++;
        dispose();
        target.unload();
      });
      return dispose;
    },
    undefined,
    target,
  );
  assertEquals(target.listeners.size, 1, "root should own one unload listener");
  target.unload();
  dispose();
  target.unload();
  assertEquals(cleanups, 1, "unload and reentrant dispose clean up once");
  assertEquals(target.listeners.size, 0, "cleanup removes its unload listener");
}

function testManualDisposalBeforeHMR() {
  const target = new LifetimeTarget();
  const hmr = hotContext();
  let cleanups = 0;
  const dispose = createRootHMR(
    (dispose) => {
      onCleanup(() => cleanups++);
      return dispose;
    },
    hmr.hot,
    target,
  );
  dispose();
  target.unload();
  hmr.trigger();
  assertEquals(cleanups, 1, "manual dispose releases the HMR root too");
  assertEquals(target.listeners.size, 0, "manual dispose detaches unload");
}

function testHMRDrainsMultipleRoots() {
  const target = new LifetimeTarget();
  const hmr = hotContext();
  let cleanups = 0;
  for (let i = 0; i < 3; i++) {
    createRootHMR(
      () => {
        onCleanup(() => {
          cleanups++;
          disposeRoot(hmr.hot);
        });
      },
      hmr.hot,
      target,
    );
  }
  assertEquals(
    hmr.registrations(),
    1,
    "one HMR callback owns all module roots",
  );
  hmr.trigger();
  target.unload();
  hmr.trigger();
  assertEquals(cleanups, 3, "HMR reentry must drain each root once");
  assertEquals(target.listeners.size, 0, "HMR removes every unload listener");
}

function testUnloadPreservesOtherHMRRoots() {
  const first = new LifetimeTarget();
  const second = new LifetimeTarget();
  const hmr = hotContext();
  let cleanups = 0;
  for (const target of [first, second]) {
    createRootHMR(() => onCleanup(() => cleanups++), hmr.hot, target);
  }
  first.unload();
  assertEquals(cleanups, 1, "closing one window leaves the other root alive");
  hmr.trigger();
  assertEquals(cleanups, 2, "HMR still releases the remaining window root");
  assertEquals(second.listeners.size, 0, "remaining listener is removed");
}

function testInitializationFailure() {
  const target = new LifetimeTarget();
  const hmr = hotContext();
  const failure = new Error("expected initialization failure");
  let cleanups = 0;
  let caught: unknown;
  try {
    createRootHMR(
      () => {
        onCleanup(() => cleanups++);
        throw failure;
      },
      hmr.hot,
      target,
    );
  } catch (error) {
    caught = error;
  }
  target.unload();
  disposeRoot(hmr.hot);
  assertEquals(caught, failure, "initialization must preserve its error");
  assertEquals(cleanups, 1, "partially initialized root is disposed once");
  assertEquals(
    target.listeners.size,
    0,
    "failed root cannot retain a listener",
  );
}

function testSynchronousDisposal() {
  const target = new LifetimeTarget();
  const hmr = hotContext();
  let cleanups = 0;
  createRootHMR(
    (dispose) => {
      onCleanup(() => cleanups++);
      dispose();
    },
    hmr.hot,
    target,
  );
  disposeRoot(hmr.hot);
  assertEquals(cleanups, 1, "disposed root is not registered again after init");
  assertEquals(hmr.registrations(), 0, "disposed root needs no HMR callback");
  assertEquals(target.listeners.size, 0, "synchronous dispose removes unload");
}

function testWithoutWindowLifetime() {
  let cleanups = 0;
  const dispose = createRootHMR(
    (dispose) => {
      onCleanup(() => cleanups++);
      return dispose;
    },
    undefined,
    null,
  );
  dispose();
  dispose();
  assertEquals(cleanups, 1, "non-window scopes support explicit disposal");
}

function testClosedWindowCannotInitialize() {
  const target = Object.assign(new LifetimeTarget(), { closed: true });
  let initialized = false;
  let rejected = false;
  try {
    createRootHMR(
      () => {
        initialized = true;
      },
      undefined,
      target,
    );
  } catch {
    rejected = true;
  }
  assertEquals(rejected, true, "late roots reject a closed owner");
  assertEquals(
    initialized,
    false,
    "closed windows must not acquire new services",
  );
  assertEquals(target.listeners.size, 0, "closed windows retain no listener");
}

function testWindowClosedDuringInitialization() {
  const target = Object.assign(new LifetimeTarget(), { closed: false });
  const hmr = hotContext();
  let cleanups = 0;
  createRootHMR(
    () => {
      onCleanup(() => cleanups++);
      target.closed = true;
    },
    hmr.hot,
    target,
  );
  assertEquals(cleanups, 1, "closing during init disposes before returning");
  assertEquals(hmr.registrations(), 0, "closed root is not tracked by HMR");
  assertEquals(target.listeners.size, 0, "closed root releases its listener");
}

export async function runAllTests(): Promise<void> {
  await runTests("rootLifecycle.test.ts", [
    {
      name: "production unload is idempotent and reentrant",
      fn: testUnloadAndReentrantCleanup,
    },
    {
      name: "manual disposal detaches unload and HMR",
      fn: testManualDisposalBeforeHMR,
    },
    {
      name: "HMR drains multiple roots despite reentry",
      fn: testHMRDrainsMultipleRoots,
    },
    {
      name: "unload preserves unrelated HMR roots",
      fn: testUnloadPreservesOtherHMRRoots,
    },
    {
      name: "failed initialization releases its root",
      fn: testInitializationFailure,
    },
    {
      name: "synchronously disposed roots stay disposed",
      fn: testSynchronousDisposal,
    },
    {
      name: "ESM scopes can omit a window lifetime",
      fn: testWithoutWindowLifetime,
    },
    {
      name: "late roots reject already closed windows",
      fn: testClosedWindowCannotInitialize,
    },
    {
      name: "closing during initialization disposes the root",
      fn: testWindowClosedDuringInitialization,
    },
  ]);
}
