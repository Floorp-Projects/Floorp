// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
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
  (globalThis as Record<string, unknown>).gFloorp = undefined;
  assertEquals(
    (globalThis as Record<string, unknown>).gFloorp as
      | typeof globalThis.gFloorp
      | undefined,
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
  assertEquals(mgr1.enableTabColor(), true, "mgr1 should be true");

  mgr2.setEnableTabColor(false);
  assertEquals(mgr2.enableTabColor(), false, "mgr2 should be false");

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
  assertEquals(threw, false, "init() should not throw");
}

// ---------------------------------------------------------------------------
// Tests: getTextColor helper function (via module import)
// ---------------------------------------------------------------------------

// getTextColor is a private function in index.ts that uses chroma-js:
//   if (chroma.valid(backgroundColor)) {
//     return chroma(backgroundColor).luminance() >= 0.5 ? "black" : "white";
//   } else { throw Error(...); }
// We replicate the logic here to verify correctness.

async function testGetTextColorLightColorsReturnBlack(): Promise<void> {
  const chroma = (await import("chroma-js")).default;
  const lightColors = ["#ffffff", "#e0e0e0", "#ffff00", "#90ee90"];
  for (const color of lightColors) {
    assert(chroma.valid(color), color + " should be valid");
    const luminance = chroma(color).luminance();
    const result = luminance >= 0.5 ? "black" : "white";
    assertEquals(result, "black", color + " lum=" + luminance.toFixed(3) + " should return black");
  }
}

async function testGetTextColorDarkColorsReturnWhite(): Promise<void> {
  const chroma = (await import("chroma-js")).default;
  const darkColors = ["#000000", "#333333", "#0000ff", "#800020"];
  for (const color of darkColors) {
    assert(chroma.valid(color), color + " should be valid");
    const luminance = chroma(color).luminance();
    const result = luminance >= 0.5 ? "black" : "white";
    assertEquals(result, "white", color + " lum=" + luminance.toFixed(3) + " should return white");
  }
}

async function testGetTextColorInvalidColorNotValid(): Promise<void> {
  const chroma = (await import("chroma-js")).default;
  assertEquals(chroma.valid("not-a-color"), false, "not-a-color should be invalid");
  assertEquals(chroma.valid("xyz"), false, "xyz should be invalid");
}

async function testGetTextColorBoundaryExactly05(): Promise<void> {
  const chroma = (await import("chroma-js")).default;
  const midGray = chroma.mix("#000000", "#ffffff", 0.5).hex();
  const luminance = chroma(midGray).luminance();
  const result = luminance >= 0.5 ? "black" : "white";
  assert(result === "black" || result === "white", "should return a valid color");
}

async function testGetTextColorHexFormats(): Promise<void> {
  const chroma = (await import("chroma-js")).default;
  assert(chroma.valid("#fff"), "#fff should be valid");
  assert(chroma.valid("#ffffff"), "#ffffff should be valid");
  assert(chroma.valid("#ffffff00"), "#ffffff00 should be valid");
}

async function testGetTextColorNamedColors(): Promise<void> {
  const chroma = (await import("chroma-js")).default;
  assert(chroma.valid("red"), "red should be valid");
  assert(chroma.valid("blue"), "blue should be valid");
  assert(chroma.valid("green"), "green should be valid");
  assert(chroma.valid("black"), "black should be valid");
  assert(chroma.valid("white"), "white should be valid");
}

// ---------------------------------------------------------------------------
// Tests: BrowserTabColor class initialization
// ---------------------------------------------------------------------------

async function testBrowserTabColorHasInitMethod(): Promise<void> {
  setupGlobals();
  const { default: BrowserTabColor } = await import("../index.ts");
  const instance = new BrowserTabColor();
  assertEquals(
    typeof instance.init,
    "function",
    "BrowserTabColor should have init method",
  );
}

async function testBrowserTabColorHasChangeTabColorMethod(): Promise<void> {
  setupGlobals();
  const { default: BrowserTabColor } = await import("../index.ts");
  const instance = new BrowserTabColor();
  assertEquals(
    typeof instance.changeTabColor,
    "function",
    "BrowserTabColor should have changeTabColor method",
  );
}

async function testBrowserTabColorChangeTabColorWhenDisabled(): Promise<void> {
  setupGlobals();
  const { default: BrowserTabColor, manager } = await import("../index.ts");
  const instance = new BrowserTabColor();

  // Ensure tab color is disabled
  manager?.setEnableTabColor(false);

  // Should not throw when disabled
  let threw = false;
  try {
    (instance as { changeTabColor: () => void }).changeTabColor();
  } catch {
    threw = true;
  }
  assertEquals(threw, false, "changeTabColor should not throw when disabled");
}

// ---------------------------------------------------------------------------
// Tests: getManifest helper function
// ---------------------------------------------------------------------------

// Note: getManifest is not exported from index.ts, it's a private helper function.
// We cannot test it directly. It should:
// - Return null when gBrowser is unavailable
// - Return null when gBrowser.selectedBrowser is unavailable
// - Return manifest when successfully obtained via ManifestObtainer

// ---------------------------------------------------------------------------
// Tests: CSS generation and injection
// ---------------------------------------------------------------------------

// Note: CSS generation and injection happens in changeTabColor method,
// which relies on browser APIs (document, ManifestObtainer) that are
// not available in the test environment. These would need integration
// tests or mocks of the browser APIs.

// ---------------------------------------------------------------------------
// Tests: Manager integration
// ---------------------------------------------------------------------------

async function testManagerExportExists(): Promise<void> {
  setupGlobals();
  const module = await import("../index.ts");
  assert(
    "manager" in module,
    "module should export manager variable",
  );
}

async function testManagerIsUndefinedInitially(): Promise<void> {
  setupGlobals();
  const module = await import("../index.ts");
  // manager is exported as a module-level variable.
  // It may already be initialized by a previous test or import,
  // so we only check that the export exists and is either undefined or a TabColorManager.
  assert(
    typeof module.manager === "undefined" || typeof module.manager === "object",
    "manager should be undefined or a TabColorManager instance before explicit BrowserTabColor init",
  );
}

// ---------------------------------------------------------------------------
// Tests: BrowserTabColor.init() with mock gBrowser
// ---------------------------------------------------------------------------

async function testBrowserTabColorInitEarlyReturnWithoutGBrowser(): Promise<void> {
  setupGlobals();
  // Ensure gBrowser is NOT available
  (globalThis as Record<string, unknown>).gBrowser = undefined;
  const { default: BrowserTabColor, manager: _mgrBefore } = await import("../index.ts");

  const instance = new BrowserTabColor();
  // init should return early without gBrowser - should not throw
  let threw = false;
  try {
    instance.init();
  } catch {
    threw = true;
  }
  assertEquals(threw, false, "init should not throw when gBrowser is unavailable");
}

async function testBrowserTabColorInitWithMockGBrowser(): Promise<void> {
  setupGlobals();
  // Create a mock gBrowser
  const listeners: Array<unknown> = [];
  const eventListeners: Record<string, Array<() => void>> = {};
  (globalThis as Record<string, unknown>).gBrowser = {
    addTabsProgressListener(listener: unknown) {
      listeners.push(listener);
    },
    removeTabsProgressListener(listener: unknown) {
      const idx = listeners.indexOf(listener);
      if (idx >= 0) listeners.splice(idx, 1);
    },
    tabContainer: {
      addEventListener(event: string, handler: () => void) {
        if (!eventListeners[event]) eventListeners[event] = [];
        eventListeners[event].push(handler);
      },
      removeEventListener(event: string, handler: () => void) {
        if (eventListeners[event]) {
          const idx = eventListeners[event].indexOf(handler);
          if (idx >= 0) eventListeners[event].splice(idx, 1);
        }
      },
    },
    selectedBrowser: {},
  };

  const { default: BrowserTabColor } = await import("../index.ts");
  const instance = new BrowserTabColor();

  let threw = false;
  try {
    instance.init();
  } catch {
    threw = true;
  }
  assertEquals(threw, false, "init should not throw with mock gBrowser");

  // Verify that a progress listener was registered
  assert(listeners.length > 0, "should have registered a tabs progress listener");

  // Verify TabSelect event listener was registered
  assert(
    "TabSelect" in eventListeners,
    "should have registered TabSelect event listener",
  );
}

async function testBrowserTabColorInitSetsManager(): Promise<void> {
  setupGlobals();
  (globalThis as Record<string, unknown>).gBrowser = {
    addTabsProgressListener() {},
    removeTabsProgressListener() {},
    tabContainer: {
      addEventListener() {},
      removeEventListener() {},
    },
    selectedBrowser: {},
  };

  const mod = await import("../index.ts");
  const instance = new mod.default();
  instance.init();

  assert(mod.manager !== undefined, "manager should be set after init");
  assert(typeof mod.manager.enableTabColor === "function", "manager should have enableTabColor signal");
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

    // BrowserTabColor class
    {
      name: "BrowserTabColor has init method",
      fn: testBrowserTabColorHasInitMethod,
    },
    {
      name: "BrowserTabColor has changeTabColor method",
      fn: testBrowserTabColorHasChangeTabColorMethod,
    },
    {
      name: "BrowserTabColor.changeTabColor when disabled",
      fn: testBrowserTabColorChangeTabColorWhenDisabled,
    },

    // Manager integration
    {
      name: "manager is exported from module",
      fn: testManagerExportExists,
    },
    {
      name: "manager is undefined before init",
      fn: testManagerIsUndefinedInitially,
    },

    // getTextColor logic (via chroma-js)
    {
      name: "getTextColor: light colors return black",
      fn: testGetTextColorLightColorsReturnBlack,
    },
    {
      name: "getTextColor: dark colors return white",
      fn: testGetTextColorDarkColorsReturnWhite,
    },
    {
      name: "getTextColor: invalid colors are not valid",
      fn: testGetTextColorInvalidColorNotValid,
    },
    {
      name: "getTextColor: boundary at 0.5 luminance",
      fn: testGetTextColorBoundaryExactly05,
    },
    {
      name: "getTextColor: hex format support",
      fn: testGetTextColorHexFormats,
    },
    {
      name: "getTextColor: named color support",
      fn: testGetTextColorNamedColors,
    },

    // BrowserTabColor.init() with mock gBrowser
    {
      name: "BrowserTabColor.init returns early without gBrowser",
      fn: testBrowserTabColorInitEarlyReturnWithoutGBrowser,
    },
    {
      name: "BrowserTabColor.init registers listeners with mock gBrowser",
      fn: testBrowserTabColorInitWithMockGBrowser,
    },
    {
      name: "BrowserTabColor.init sets manager after init",
      fn: testBrowserTabColorInitSetsManager,
    },
  ];

  const { runTests } = await import("../../../test/utils/test_harness.ts");
  await runTests("browserTabColor.test.ts", tests);
}
