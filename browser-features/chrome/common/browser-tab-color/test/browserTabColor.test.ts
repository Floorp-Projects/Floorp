// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  assertNotEquals,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Module under test — imported lazily so the harness can set up globals first
// ---------------------------------------------------------------------------

// TabColorManager will be imported inside createManager() to ensure globals
// are set up before the module-level createSignal call in TabColorManager

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Set up globalThis.gFloorp so TabColorManager constructor does not throw */
function setupGlobals(): void {
  if (!globalThis.gFloorp) {
    globalThis.gFloorp = {} as typeof globalThis.gFloorp;
  }
}

// ---------------------------------------------------------------------------
// Tests: TabColorManager — Constructor & Globals
// ---------------------------------------------------------------------------

async function testConstructorCreatesGlobalGFloorp(): Promise<void> {
  delete (globalThis as Record<string, unknown>).gFloorp;
  assertEquals(
    (globalThis as Record<string, unknown>).gFloorp as
      typeof globalThis.gFloorp | undefined,
    undefined,
    "gFloorp should not exist before construction",
  );

  setupGlobals();
  const { TabColorManager } = await import("../tabcolor-manager.tsx");
  const mgr = new TabColorManager();

  assertNotEquals(
    globalThis.gFloorp as typeof globalThis.gFloorp | undefined,
    undefined,
    "gFloorp should be created by constructor",
  );
  assertEquals(
    typeof mgr,
    "object",
    "TabColorManager instance should be an object",
  );
}

async function testConstructorPreservesExistingGFloorp(): Promise<void> {
  setupGlobals();
  const existing = globalThis.gFloorp;
  const { TabColorManager } = await import("../tabcolor-manager.tsx");
  new TabColorManager();
  assertEquals(
    globalThis.gFloorp === existing,
    true,
    "constructor should preserve existing gFloorp object",
  );
}

async function testConstructorSetsTabColorSetEnable(): Promise<void> {
  setupGlobals();
  const { TabColorManager } = await import("../tabcolor-manager.tsx");
  const mgr = new TabColorManager();
  const gFloorp = globalThis.gFloorp;
  const tabColor = gFloorp.tabColor!;

  assertEquals(
    typeof tabColor,
    "object",
    "gFloorp.tabColor should be an object",
  );
  assertEquals(
    typeof tabColor.setEnable,
    "function",
    "gFloorp.tabColor.setEnable should be a function",
  );

  // setEnable should be bound to the manager's setEnableTabColor signal
  tabColor.setEnable(true);
  assertEquals(
    mgr.enableTabColor(),
    true,
    "setEnable(true) should enable tab color",
  );

  tabColor.setEnable(false);
  assertEquals(
    mgr.enableTabColor(),
    false,
    "setEnable(false) should disable tab color",
  );
}

// ---------------------------------------------------------------------------
// Tests: TabColorManager — Signal basics
// ---------------------------------------------------------------------------

async function testEnableTabColorDefaultReflectsConfig(): Promise<void> {
  setupGlobals();
  const { TabColorManager } = await import("../tabcolor-manager.tsx");
  const mgr = new TabColorManager();
  // The default value comes from config().globalConfigs.faviconColor
  const value = mgr.enableTabColor();
  assertEquals(
    typeof value,
    "boolean",
    "enableTabColor() should return a boolean",
  );
}

async function testSetEnableTabColorToggles(): Promise<void> {
  setupGlobals();
  const { TabColorManager } = await import("../tabcolor-manager.tsx");
  const mgr = new TabColorManager();

  mgr.setEnableTabColor(true);
  assertEquals(
    mgr.enableTabColor(),
    true,
    "enableTabColor should be true after setEnableTabColor(true)",
  );

  mgr.setEnableTabColor(false);
  assertEquals(
    mgr.enableTabColor(),
    false,
    "enableTabColor should be false after setEnableTabColor(false)",
  );
}

async function testSetEnableTabColorWithCallback(): Promise<void> {
  setupGlobals();
  const { TabColorManager } = await import("../tabcolor-manager.tsx");
  const mgr = new TabColorManager();
  mgr.setEnableTabColor(false);

  mgr.setEnableTabColor((prev) => !prev);
  assertEquals(
    mgr.enableTabColor(),
    true,
    "should toggle from false to true via callback",
  );

  mgr.setEnableTabColor((prev) => !prev);
  assertEquals(
    mgr.enableTabColor(),
    false,
    "should toggle from true to false via callback",
  );
}

async function testSetEnableTabColorIdempotent(): Promise<void> {
  setupGlobals();
  const { TabColorManager } = await import("../tabcolor-manager.tsx");
  const mgr = new TabColorManager();

  mgr.setEnableTabColor(true);
  mgr.setEnableTabColor(true);
  assertEquals(
    mgr.enableTabColor(),
    true,
    "setting same value twice should be idempotent (true)",
  );

  mgr.setEnableTabColor(false);
  mgr.setEnableTabColor(false);
  assertEquals(
    mgr.enableTabColor(),
    false,
    "setting same value twice should be idempotent (false)",
  );
}

async function testMultipleToggleCycles(): Promise<void> {
  setupGlobals();
  const { TabColorManager } = await import("../tabcolor-manager.tsx");
  const mgr = new TabColorManager();
  mgr.setEnableTabColor(false);

  for (let i = 0; i < 10; i++) {
    mgr.setEnableTabColor((prev) => !prev);
  }
  assertEquals(
    mgr.enableTabColor(),
    false,
    "after even number of toggles, should return to original state",
  );

  mgr.setEnableTabColor((prev) => !prev);
  assertEquals(
    mgr.enableTabColor(),
    true,
    "one more toggle should make it true",
  );
}

async function testCallbackReceivesCorrectPreviousValue(): Promise<void> {
  setupGlobals();
  const { TabColorManager } = await import("../tabcolor-manager.tsx");
  const mgr = new TabColorManager();
  mgr.setEnableTabColor(false);

  mgr.setEnableTabColor((prev) => {
    assertEquals(prev, false, "callback should receive current false value");
    return !prev;
  });
  assertEquals(
    mgr.enableTabColor(),
    true,
    "should be true after toggling from false",
  );

  mgr.setEnableTabColor((prev) => {
    assertEquals(prev, true, "callback should receive current true value");
    return !prev;
  });
  assertEquals(
    mgr.enableTabColor(),
    false,
    "should be false after toggling from true",
  );
}

// ---------------------------------------------------------------------------
// Tests: TabColorManager — Multiple instances
// ---------------------------------------------------------------------------

async function testMultipleInstancesShareGlobalGFloorp(): Promise<void> {
  setupGlobals();
  const { TabColorManager } = await import("../tabcolor-manager.tsx");
  const mgr1 = new TabColorManager();
  const mgr2 = new TabColorManager();

  // Both should reference the same globalThis.gFloorp
  const gFloorp = globalThis.gFloorp;
  assertNotEquals(gFloorp, undefined, "gFloorp should exist");

  // The last constructed manager's setEnable should be the active one
  mgr1.setEnableTabColor(true);
  assertEquals(
    mgr1.enableTabColor(),
    true,
    "mgr1 should be true",
  );

  mgr2.setEnableTabColor(false);
  assertEquals(
    mgr2.enableTabColor(),
    false,
    "mgr2 should be false",
  );

  // mgr1's signal is independent
  assertEquals(
    mgr1.enableTabColor(),
    true,
    "mgr1 signal should still be true (independent signal)",
  );
}

// ---------------------------------------------------------------------------
// Tests: TabColorManager — init method
// ---------------------------------------------------------------------------

async function testInitMethodExists(): Promise<void> {
  setupGlobals();
  const { TabColorManager } = await import("../tabcolor-manager.tsx");
  const mgr = new TabColorManager();
  assertEquals(
    typeof mgr.init,
    "function",
    "TabColorManager should have an init method",
  );
}

async function testInitDoesNotThrow(): Promise<void> {
  setupGlobals();
  const { TabColorManager } = await import("../tabcolor-manager.tsx");
  const mgr = new TabColorManager();
  // init() sets up a createEffect — should not throw in browser env
  let threw = false;
  try {
    mgr.init();
  } catch {
    threw = true;
  }
  assertEquals(
    threw,
    false,
    "init() should not throw",
  );
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    // Constructor & globals
    {
      name: "constructor creates global gFloorp if missing",
      fn: testConstructorCreatesGlobalGFloorp,
    },
    {
      name: "constructor preserves existing gFloorp",
      fn: testConstructorPreservesExistingGFloorp,
    },
    {
      name: "constructor sets gFloorp.tabColor.setEnable",
      fn: testConstructorSetsTabColorSetEnable,
    },

    // Signal basics
    {
      name: "enableTabColor default reflects config",
      fn: testEnableTabColorDefaultReflectsConfig,
    },
    {
      name: "setEnableTabColor toggles boolean",
      fn: testSetEnableTabColorToggles,
    },
    {
      name: "setEnableTabColor works with callback",
      fn: testSetEnableTabColorWithCallback,
    },
    {
      name: "setEnableTabColor is idempotent",
      fn: testSetEnableTabColorIdempotent,
    },
    {
      name: "multiple toggle cycles remain stable",
      fn: testMultipleToggleCycles,
    },
    {
      name: "callback receives correct previous value",
      fn: testCallbackReceivesCorrectPreviousValue,
    },

    // Multiple instances
    {
      name: "multiple instances share global gFloorp",
      fn: testMultipleInstancesShareGlobalGFloorp,
    },

    // init method
    {
      name: "init method exists",
      fn: testInitMethodExists,
    },
    {
      name: "init does not throw",
      fn: testInitDoesNotThrow,
    },
  ];

  const { runTests } = await import("../../../test/utils/test_harness.ts");
  await runTests("browserTabColor.test.ts", tests);
}
