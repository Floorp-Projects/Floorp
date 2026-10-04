// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { signal } from "@preact/signals";
import type { ViteHotContext } from "vite/types/hot.js";
import {
  addDisposer,
  createNodeDisposer,
  createRoot,
  createRootHMR,
  disposeRoot,
  rootEffect,
} from "../lifetime.ts";
import {
  assertEquals,
  assertThrows,
  runTests,
} from "../../../browser-features/chrome/test/utils/test_harness.ts";

function hotContext(external = false) {
  const callbacks: (() => void)[] = [];
  const hot = {
    data: { __preactXulExternalDisposeOwner: external },
    dispose: (callback: () => void) => callbacks.push(callback),
  } as unknown as ViteHotContext;
  return { hot, callbacks };
}

function testMultipleHMRRoots(): void {
  const { hot, callbacks } = hotContext();
  const state = signal(0);
  let runs = 0;
  let cleanups = 0;
  for (let index = 0; index < 2; index++) {
    createRootHMR(() => {
      rootEffect(() => {
        state.value;
        runs++;
        return () => cleanups++;
      });
    }, hot);
  }
  assertEquals(callbacks.length, 1, "only one dispose callback per module");
  state.value++;
  assertEquals(runs, 4, "both roots react before HMR");
  callbacks[0]();
  state.value++;
  assertEquals(runs, 4, "all roots stop reacting after HMR");
  assertEquals(
    cleanups,
    4,
    "each effect update and final subscription cleans up",
  );
  callbacks[0]();
  assertEquals(cleanups, 4, "HMR disposal is idempotent");
}

function testExternalHMROwnerAndManualRoot(): void {
  const { hot, callbacks } = hotContext(true);
  let cleanups = 0;
  createRootHMR(() => addDisposer(() => cleanups++), hot);
  assertEquals(
    callbacks.length,
    0,
    "base class retains ownership of HMR callback",
  );
  disposeRoot(hot);
  disposeRoot(hot);
  assertEquals(cleanups, 1, "base class can drain roots exactly once");
  const value = createRoot((dispose) => {
    addDisposer(() => cleanups++);
    dispose();
    dispose();
    addDisposer(() => cleanups++);
    return "result";
  });
  assertEquals(value, "result", "manual root returns its callback value");
  assertEquals(
    cleanups,
    3,
    "late setup in a disposed scope cleans up immediately",
  );
}

function testFailedSetupAndCleanupIsolation(): void {
  const state = signal(0);
  let runs = 0;
  let cleanups = 0;
  assertThrows(() =>
    createRoot(() => {
      rootEffect(() => {
        state.value;
        runs++;
      });
      addDisposer(() => cleanups++);
      throw new Error("setup failure");
    }), "failed setup propagates its error");
  state.value++;
  assertEquals(runs, 1, "failed setup disposes existing effects");
  assertEquals(cleanups, 1, "failed setup disposes registered resources");

  const originalError = console.error;
  let logged = 0;
  console.error = () => logged++;
  try {
    createRoot((dispose) => {
      addDisposer(() => cleanups++);
      addDisposer(() => {
        throw new Error("cleanup failure");
      });
      createRoot(() => addDisposer(() => cleanups++));
      dispose();
    });
  } finally {
    console.error = originalError;
  }
  assertEquals(logged, 1, "failed cleanup is reported");
  assertEquals(
    cleanups,
    3,
    "nested roots and other cleanup survive a failing disposer",
  );
}

async function mutationCheckpoint(): Promise<void> {
  await Promise.resolve();
  await Promise.resolve();
}

async function testNodeLifecycle(): Promise<void> {
  const parent = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "div",
  );
  const destination = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "div",
  );
  const node = document.createElementNS("http://www.w3.org/1999/xhtml", "span");
  document.documentElement.append(parent, destination);
  let cleanups = 0;
  const dispose = createNodeDisposer(node, () => cleanups++);
  try {
    await mutationCheckpoint();
    assertEquals(cleanups, 0, "onCreated may run before insertion");
    parent.appendChild(node);
    await mutationCheckpoint();
    destination.appendChild(node);
    await mutationCheckpoint();
    assertEquals(
      cleanups,
      0,
      "moving a connected widget preserves its effects",
    );
    destination.remove();
    await mutationCheckpoint();
    assertEquals(cleanups, 1, "removing an ancestor disposes the widget");
    dispose();
    assertEquals(cleanups, 1, "manual disposal after removal is idempotent");
  } finally {
    dispose();
    parent.remove();
    destination.remove();
  }
}

async function testNodeInsertedAndRemovedInSameBatch(): Promise<void> {
  const node = document.createElementNS("http://www.w3.org/1999/xhtml", "span");
  let cleanups = 0;
  const dispose = createNodeDisposer(node, () => cleanups++);
  try {
    document.documentElement.appendChild(node);
    node.remove();
    await mutationCheckpoint();
    assertEquals(
      cleanups,
      1,
      "observed removal detects a short-lived initial attachment",
    );
  } finally {
    dispose();
    node.remove();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("preact-xul/lifetime.test.ts", [
    {
      name: "multiple HMR roots share one callback and all dispose",
      fn: testMultipleHMRRoots,
    },
    {
      name: "external HMR ownership and manual root cleanup",
      fn: testExternalHMROwnerAndManualRoot,
    },
    {
      name: "failed setup and failed cleanup release other resources",
      fn: testFailedSetupAndCleanupIsolation,
    },
    {
      name:
        "node lifetime handles delayed insertion, moves and ancestor removal",
      fn: testNodeLifecycle,
    },
    {
      name: "short-lived node attachment still disposes",
      fn: testNodeInsertedAndRemovedInSameBatch,
    },
  ]);
}
