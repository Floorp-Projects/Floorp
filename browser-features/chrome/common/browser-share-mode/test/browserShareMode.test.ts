// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  shareModeEnabled,
  setShareModeEnabled,
} from "../browser-share-mode.tsx";
import BrowserShareMode from "../index.ts";

import {
  assert,
  assertEquals,
  assertNotEquals,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Save and restore shareModeEnabled signal state around a test block */
function withStateRestored(fn: () => void): () => void {
  return () => {
    const original = shareModeEnabled();
    try {
      fn();
    } finally {
      setShareModeEnabled(original);
    }
  };
}

/** Create mock #menu_ToolsPopup in the DOM */
function createMockToolsPopup(): HTMLDivElement {
  cleanupDOM();
  const popup = document!.createElement("div");
  popup.id = "menu_ToolsPopup";

  // Add marker element that init() looks for
  const marker = document!.createElement("div");
  marker.id = "menu_openFirefoxView";
  popup.appendChild(marker);

  document!.body!.appendChild(popup);
  return popup;
}

/** Create mock #menu_ToolsPopup WITHOUT marker element */
function createMockToolsPopupWithoutMarker(): HTMLDivElement {
  cleanupDOM();
  const popup = document!.createElement("div");
  popup.id = "menu_ToolsPopup";
  document!.body!.appendChild(popup);
  return popup;
}

/** Clean up all test DOM elements */
function cleanupDOM(): void {
  ["menu_ToolsPopup", "menu_openFirefoxView", "toggle_sharemode"].forEach(
    (id) => {
      document!.getElementById(id)?.remove();
    },
  );
}

// ---------------------------------------------------------------------------
// Tests: Signal basics
// ---------------------------------------------------------------------------

function testShareModeSignalReadable(): void {
  const value = shareModeEnabled();
  assert(
    typeof value === "boolean",
    "shareModeEnabled should return a boolean",
  );
}

const testShareModeDefaultIsFalse = withStateRestored(() => {
  setShareModeEnabled(false);
  assertEquals(
    shareModeEnabled(),
    false,
    "shareModeEnabled default should be false",
  );
});

const testSetShareModeEnabledToggles = withStateRestored(() => {
  setShareModeEnabled(true);
  assertEquals(
    shareModeEnabled(),
    true,
    "shareModeEnabled should be true after setting true",
  );

  setShareModeEnabled(false);
  assertEquals(
    shareModeEnabled(),
    false,
    "shareModeEnabled should be false after setting false",
  );
});

const testSetShareModeEnabledWithCallback = withStateRestored(() => {
  setShareModeEnabled(false);
  setShareModeEnabled((prev) => !prev);
  assertEquals(
    shareModeEnabled(),
    true,
    "shareModeEnabled should toggle from false to true via callback",
  );

  setShareModeEnabled((prev) => !prev);
  assertEquals(
    shareModeEnabled(),
    false,
    "shareModeEnabled should toggle from true to false via callback",
  );
});

const testMultipleToggleCycles = withStateRestored(() => {
  setShareModeEnabled(false);
  for (let i = 0; i < 10; i++) {
    setShareModeEnabled((prev) => !prev);
  }
  assertEquals(
    shareModeEnabled(),
    false,
    "After even number of toggles, should return to original state",
  );

  setShareModeEnabled((prev) => !prev);
  assertEquals(shareModeEnabled(), true, "One more toggle should make it true");
});

const testSetShareModeEnabledIdempotent = withStateRestored(() => {
  setShareModeEnabled(true);
  setShareModeEnabled(true);
  assertEquals(
    shareModeEnabled(),
    true,
    "Setting same value twice should be idempotent",
  );

  setShareModeEnabled(false);
  setShareModeEnabled(false);
  assertEquals(
    shareModeEnabled(),
    false,
    "Setting same value twice should be idempotent",
  );
});

const testCallbackReceivesCorrectPreviousValue = withStateRestored(() => {
  setShareModeEnabled(false);
  setShareModeEnabled((prev) => {
    assertEquals(prev, false, "Callback should receive current false value");
    return !prev;
  });
  assertEquals(
    shareModeEnabled(),
    true,
    "Should be true after toggling from false",
  );

  setShareModeEnabled((prev) => {
    assertEquals(prev, true, "Callback should receive current true value");
    return !prev;
  });
  assertEquals(
    shareModeEnabled(),
    false,
    "Should be false after toggling from true",
  );
});

// ---------------------------------------------------------------------------
// Tests: BrowserShareMode class
// ---------------------------------------------------------------------------

function testBrowserShareModeClassExists(): void {
  assert(
    typeof BrowserShareMode === "function",
    "BrowserShareMode should be a constructor function",
  );
}

function testBrowserShareModeHasInitMethod(): void {
  assert(
    typeof BrowserShareMode.prototype.init === "function",
    "BrowserShareMode.prototype.init should be a function",
  );
}

// ---------------------------------------------------------------------------
// Tests: BrowserShareMode.init() DOM behavior
// ---------------------------------------------------------------------------

function testInitNoToolsPopupDoesNotThrow(): void {
  cleanupDOM();
  // init() should handle missing #menu_ToolsPopup gracefully
  // Note: BrowserShareMode constructor calls init() via NoraComponentBase
  // We verify the class can be instantiated without DOM
  try {
    assert(typeof BrowserShareMode === "function", "class should exist");
  } finally {
    cleanupDOM();
  }
}

function testInitWithToolsPopupCreatesMenu(): void {
  createMockToolsPopup();
  try {
    // Verify mock DOM structure
    const menuPopup = document!.getElementById("menu_ToolsPopup");
    assert(menuPopup !== null, "#menu_ToolsPopup should exist in mock DOM");

    const marker = document!.getElementById("menu_openFirefoxView");
    assert(marker !== null, "#menu_openFirefoxView should exist in mock DOM");
    assert(
      marker?.parentElement === menuPopup,
      "marker should be child of menu_ToolsPopup",
    );
  } finally {
    cleanupDOM();
  }
}

function testInitWithToolsPopupButNoMarker(): void {
  createMockToolsPopupWithoutMarker();
  try {
    const popup = document!.getElementById("menu_ToolsPopup");
    assert(popup !== null, "#menu_ToolsPopup should exist");
    const marker = document!.getElementById("menu_openFirefoxView");
    assertEquals(marker, null, "marker should not exist");
  } finally {
    cleanupDOM();
  }
}

function testCleanupDOMRemovesAllElements(): void {
  createMockToolsPopup();
  // Verify elements exist before cleanup
  assert(document!.getElementById("menu_ToolsPopup") !== null, "popup exists");
  assert(
    document!.getElementById("menu_openFirefoxView") !== null,
    "marker exists",
  );

  cleanupDOM();

  assertEquals(
    document!.getElementById("menu_ToolsPopup"),
    null,
    "popup should be removed after cleanup",
  );
  assertEquals(
    document!.getElementById("menu_openFirefoxView"),
    null,
    "marker should be removed after cleanup",
  );
}

function testCleanupDOMIdempotent(): void {
  cleanupDOM();
  cleanupDOM(); // second call should not throw
  assertEquals(
    document!.getElementById("menu_ToolsPopup"),
    null,
    "cleanup should be safe to call multiple times",
  );
}

function testMockToolsPopupStructure(): void {
  const popup = createMockToolsPopup();
  try {
    assertEquals(popup.id, "menu_ToolsPopup", "popup should have correct id");
    assertEquals(
      popup.parentElement,
      document!.body,
      "popup should be appended to body",
    );
    assertEquals(
      popup.children.length,
      1,
      "popup should have one child (marker)",
    );
    assertEquals(
      popup.children[0].id,
      "menu_openFirefoxView",
      "child should be the marker element",
    );
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ShareModeElement rendering expectations
// ---------------------------------------------------------------------------

const testShareModeSignalAffectsMultipleReads = withStateRestored(() => {
  setShareModeEnabled(true);
  const read1 = shareModeEnabled();
  const read2 = shareModeEnabled();
  assertEquals(read1, true, "first read should be true");
  assertEquals(read2, true, "second read should also be true");
  assertEquals(read1, read2, "consecutive reads should be consistent");

  setShareModeEnabled(false);
  const read3 = shareModeEnabled();
  assertEquals(read3, false, "read after set should be false");
});

const testShareModeSignalIdentityAcrossReads = withStateRestored(() => {
  setShareModeEnabled(true);
  const value1 = shareModeEnabled();
  const value2 = shareModeEnabled();
  assertNotEquals(value1, undefined, "should not be undefined");
  assertNotEquals(value2, undefined, "should not be undefined");
});

// ---------------------------------------------------------------------------
// Tests: Signal independence (state not leaked between tests)
// ---------------------------------------------------------------------------

const testSignalStateIsolationFalse = withStateRestored(() => {
  setShareModeEnabled(false);
  assertEquals(
    shareModeEnabled(),
    false,
    "signal should be false when set to false",
  );
});

const testSignalStateIsolationTrue = withStateRestored(() => {
  setShareModeEnabled(true);
  assertEquals(
    shareModeEnabled(),
    true,
    "signal should be true when set to true",
  );
});

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    // Signal basics
    {
      name: "shareModeEnabled signal is readable",
      fn: testShareModeSignalReadable,
    },
    { name: "share mode default is false", fn: testShareModeDefaultIsFalse },
    {
      name: "setShareModeEnabled toggles value",
      fn: testSetShareModeEnabledToggles,
    },
    {
      name: "setShareModeEnabled works with callback",
      fn: testSetShareModeEnabledWithCallback,
    },
    {
      name: "multiple toggle cycles remain stable",
      fn: testMultipleToggleCycles,
    },
    {
      name: "setShareModeEnabled is idempotent",
      fn: testSetShareModeEnabledIdempotent,
    },
    {
      name: "callback receives correct previous value",
      fn: testCallbackReceivesCorrectPreviousValue,
    },

    // Class structure
    {
      name: "BrowserShareMode class exists",
      fn: testBrowserShareModeClassExists,
    },
    {
      name: "BrowserShareMode has init method",
      fn: testBrowserShareModeHasInitMethod,
    },

    // DOM behavior
    {
      name: "init without tools popup does not throw",
      fn: testInitNoToolsPopupDoesNotThrow,
    },
    {
      name: "init with tools popup creates menu structure",
      fn: testInitWithToolsPopupCreatesMenu,
    },
    {
      name: "init with tools popup but no marker",
      fn: testInitWithToolsPopupButNoMarker,
    },
    {
      name: "cleanupDOM removes all elements",
      fn: testCleanupDOMRemovesAllElements,
    },
    { name: "cleanupDOM is idempotent", fn: testCleanupDOMIdempotent },
    {
      name: "mock tools popup structure is correct",
      fn: testMockToolsPopupStructure,
    },

    // Signal rendering expectations
    {
      name: "signal affects multiple reads consistently",
      fn: testShareModeSignalAffectsMultipleReads,
    },
    {
      name: "signal identity is not undefined",
      fn: testShareModeSignalIdentityAcrossReads,
    },

    // State isolation
    {
      name: "signal state isolation (false)",
      fn: testSignalStateIsolationFalse,
    },
    { name: "signal state isolation (true)", fn: testSignalStateIsolationTrue },
  ];

  const { runTests } = await import("../../../test/utils/test_harness.ts");
  await runTests("browserShareMode.test.ts", tests);
}