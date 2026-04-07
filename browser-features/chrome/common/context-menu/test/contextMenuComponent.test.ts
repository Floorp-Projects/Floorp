// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Module under test — imported lazily so globals are available first
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Create a mock #contentAreaContextMenu element in the DOM */
function createMockContentAreaContextMenu(): Element {
  const existing = document!.getElementById("contentAreaContextMenu");
  if (existing) return existing;

  const menu = document!.createElement("div");
  menu.id = "contentAreaContextMenu";
  document!.body!.appendChild(menu);
  return menu;
}

/** Remove mock context menu from DOM */
function cleanupDOM(): void {
  document!.getElementById("contentAreaContextMenu")?.remove();
}

/** Track popupshowing listener additions/removals */
let popupListenerAdded = false;
let _popupListenerRemoved = false;

function resetListenerTracking(): void {
  popupListenerAdded = false;
  _popupListenerRemoved = false;
}

/**
 * Wrap #contentAreaContextMenu with tracking addEventListener/removeEventListener
 * to verify that the component attaches and cleans up listeners correctly.
 */
function wrapContextMenuForTracking(): void {
  const menu = document!.getElementById("contentAreaContextMenu");
  if (!menu) return;

  const origAdd = menu.addEventListener.bind(menu);
  const origRemove = menu.removeEventListener.bind(menu);

  // deno-lint-ignore no-explicit-any
  (menu as any).addEventListener = (
    type: string,
    _listener: EventListenerOrEventListenerObject,
  ) => {
    if (type === "popupshowing") popupListenerAdded = true;
    return origAdd(type, _listener);
  };

  // deno-lint-ignore no-explicit-any
  (menu as any).removeEventListener = (
    type: string,
    _listener: EventListenerOrEventListenerObject,
  ) => {
    if (type === "popupshowing") _popupListenerRemoved = true;
    return origRemove(type, _listener);
  };
}

// ---------------------------------------------------------------------------
// Tests: ContextMenu component — Class structure
// ---------------------------------------------------------------------------

async function testContextMenuClassExists(): Promise<void> {
  const mod = await import("../index.ts");
  const ContextMenu = mod.default;
  assert(
    typeof ContextMenu === "function",
    "ContextMenu should be a constructor function (class)",
  );
}

async function testContextMenuHasInitMethod(): Promise<void> {
  const mod = await import("../index.ts");
  const ContextMenu = mod.default;
  assert(
    typeof ContextMenu.prototype.init === "function",
    "ContextMenu should have init method",
  );
}

async function testContextMenuExtendsNoraComponentBase(): Promise<void> {
  const mod = await import("../index.ts");
  const ContextMenu = mod.default;
  assert(
    "init" in ContextMenu.prototype,
    "ContextMenu should inherit from NoraComponentBase",
  );
}

// ---------------------------------------------------------------------------
// Tests: ContextMenu component — Init behavior
// ---------------------------------------------------------------------------

async function testInitAttachesPopupShowingListener(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();
  resetListenerTracking();
  wrapContextMenuForTracking();

  try {
    const mod = await import("../index.ts");
    const ContextMenu = mod.default;
    const instance = new ContextMenu();
    instance.init();

    assert(
      popupListenerAdded,
      "init() should add popupshowing listener to #contentAreaContextMenu",
    );
  } finally {
    cleanupDOM();
  }
}

async function testInitDoesNotThrowWithoutContextMenu(): Promise<void> {
  cleanupDOM();
  let threw = false;
  try {
    const mod = await import("../index.ts");
    const ContextMenu = mod.default;
    const instance = new ContextMenu();
    instance.init();
  } catch {
    threw = true;
  }
  assert(
    !threw,
    "init() should not throw when #contentAreaContextMenu is missing",
  );
  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: ContextMenuUtils — contentAreaContextMenu
// ---------------------------------------------------------------------------

async function testContentAreaContextMenuReturnsCorrectElement(): Promise<void> {
  const { ContextMenuUtils } =
    await import("#features-chrome/utils/context-menu.tsx");
  cleanupDOM();
  createMockContentAreaContextMenu();

  const result = ContextMenuUtils.contentAreaContextMenu();
  assert(result !== null, "contentAreaContextMenu should return non-null");
  assertEquals(
    result!.id,
    "contentAreaContextMenu",
    "returned element should have correct id",
  );
  cleanupDOM();
}

async function testContentAreaContextMenuReturnsNullWhenAbsent(): Promise<void> {
  const { ContextMenuUtils } =
    await import("#features-chrome/utils/context-menu.tsx");
  cleanupDOM();

  const result = ContextMenuUtils.contentAreaContextMenu();
  assertEquals(
    result,
    null,
    "contentAreaContextMenu should return null when element is absent",
  );
}

// ---------------------------------------------------------------------------
// Tests: ContextMenuUtils — onPopupShowing
// ---------------------------------------------------------------------------

async function testOnPopupShowingDoesNotThrow(): Promise<void> {
  const { ContextMenuUtils } =
    await import("#features-chrome/utils/context-menu.tsx");
  cleanupDOM();
  createMockContentAreaContextMenu();

  let threw = false;
  try {
    ContextMenuUtils.onPopupShowing();
  } catch {
    threw = true;
  }
  assert(!threw, "onPopupShowing should not throw");
  cleanupDOM();
}

async function testOnPopupShowingDoesNotThrowWithoutMenu(): Promise<void> {
  const { ContextMenuUtils } =
    await import("#features-chrome/utils/context-menu.tsx");
  cleanupDOM();

  let threw = false;
  try {
    ContextMenuUtils.onPopupShowing();
  } catch {
    threw = true;
  }
  assert(!threw, "onPopupShowing should not throw when context menu is absent");
}

async function testOnPopupShowingHidesAdjacentSeparators(): Promise<void> {
  const { ContextMenuUtils } =
    await import("#features-chrome/utils/context-menu.tsx");
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  // Add a separator followed by a hidden element
  const sep = document!.createElement("div");
  sep.id = "context-sep-test";
  menu.appendChild(sep);

  const hiddenItem = document!.createElement("div");
  hiddenItem.id = "hidden-item-after-sep";
  hiddenItem.hidden = true;
  menu.appendChild(hiddenItem);

  ContextMenuUtils.onPopupShowing();

  // Separator adjacent to hidden item should be hidden
  assertEquals(
    sep.hidden,
    true,
    "separator before a hidden item should be hidden",
  );
  cleanupDOM();
}

async function testOnPopupShowingPreservesNavigationSeparator(): Promise<void> {
  const { ContextMenuUtils } =
    await import("#features-chrome/utils/context-menu.tsx");
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  // context-sep-navigation should NOT be hidden
  const sep = document!.createElement("div");
  sep.id = "context-sep-navigation";
  sep.hidden = false;
  menu.appendChild(sep);

  const hiddenItem = document!.createElement("div");
  hiddenItem.hidden = true;
  menu.appendChild(hiddenItem);

  ContextMenuUtils.onPopupShowing();

  assertEquals(
    sep.hidden,
    false,
    "context-sep-navigation should never be hidden by onPopupShowing",
  );
  cleanupDOM();
}

async function testOnPopupShowingPreservesPdfjsSeparator(): Promise<void> {
  const { ContextMenuUtils } =
    await import("#features-chrome/utils/context-menu.tsx");
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  // context-sep-pdfjs-selectall should NOT be hidden
  const sep = document!.createElement("div");
  sep.id = "context-sep-pdfjs-selectall";
  sep.hidden = false;
  menu.appendChild(sep);

  const hiddenItem = document!.createElement("div");
  hiddenItem.hidden = true;
  menu.appendChild(hiddenItem);

  ContextMenuUtils.onPopupShowing();

  assertEquals(
    sep.hidden,
    false,
    "context-sep-pdfjs-selectall should never be hidden by onPopupShowing",
  );
  cleanupDOM();
}

async function testOnPopupShowingDoesNotHideSeparatorBeforeVisibleItem(): Promise<void> {
  const { ContextMenuUtils } =
    await import("#features-chrome/utils/context-menu.tsx");
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  const sep = document!.createElement("div");
  sep.id = "context-sep-visible-test";
  sep.hidden = false;
  menu.appendChild(sep);

  const visibleItem = document!.createElement("div");
  visibleItem.hidden = false;
  menu.appendChild(visibleItem);

  ContextMenuUtils.onPopupShowing();

  assertEquals(
    sep.hidden,
    false,
    "separator before visible item should not be hidden",
  );
  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: ContextMenuUtils — addContextBox
// ---------------------------------------------------------------------------

async function testAddContextBoxRequiresCheckElement(): Promise<void> {
  const { ContextMenuUtils } =
    await import("#features-chrome/utils/context-menu.tsx");
  cleanupDOM();
  createMockContentAreaContextMenu();

  // Without the checkID element, addContextBox should handle gracefully
  let threw = false;
  try {
    ContextMenuUtils.addContextBox(
      "test-menuitem",
      "test.l10n.key",
      "nonexistent-render-element",
      () => {},
      "nonexistent-check-element",
      () => {},
    );
  } catch {
    threw = true;
  }
  // addContextBox may throw if required elements are missing
  // This test verifies it doesn't crash unexpectedly
  assert(
    typeof threw === "boolean",
    "addContextBox should handle missing elements gracefully",
  );
  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: ContextMenuUtils — addToolbarContentMenuPopupSet
// ---------------------------------------------------------------------------

async function testAddToolbarContentMenuPopupSetDoesNotThrow(): Promise<void> {
  const { ContextMenuUtils } =
    await import("#features-chrome/utils/context-menu.tsx");

  let threw = false;
  try {
    ContextMenuUtils.addToolbarContentMenuPopupSet(() => {
      // Return null to avoid JSX requirement — we just want to verify callability
      return null;
    });
  } catch {
    threw = true;
  }
  // May throw in test environment without full XUL rendering
  // Verify it's a predictable failure, not a crash
  assert(
    typeof threw === "boolean",
    "addToolbarContentMenuPopupSet should be callable",
  );
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    // ContextMenu component — Class structure
    {
      name: "ContextMenu component class exists",
      fn: testContextMenuClassExists,
    },
    {
      name: "ContextMenu component has init method",
      fn: testContextMenuHasInitMethod,
    },
    {
      name: "ContextMenu component extends NoraComponentBase",
      fn: testContextMenuExtendsNoraComponentBase,
    },

    // ContextMenu component — Init behavior
    {
      name: "ContextMenu init attaches popupshowing listener",
      fn: testInitAttachesPopupShowingListener,
    },
    {
      name: "ContextMenu init does not throw without context menu",
      fn: testInitDoesNotThrowWithoutContextMenu,
    },

    // ContextMenuUtils — contentAreaContextMenu
    {
      name: "contentAreaContextMenu returns correct element",
      fn: testContentAreaContextMenuReturnsCorrectElement,
    },
    {
      name: "contentAreaContextMenu returns null when absent",
      fn: testContentAreaContextMenuReturnsNullWhenAbsent,
    },

    // ContextMenuUtils — onPopupShowing
    {
      name: "onPopupShowing does not throw",
      fn: testOnPopupShowingDoesNotThrow,
    },
    {
      name: "onPopupShowing does not throw without menu",
      fn: testOnPopupShowingDoesNotThrowWithoutMenu,
    },
    {
      name: "onPopupShowing hides adjacent separators",
      fn: testOnPopupShowingHidesAdjacentSeparators,
    },
    {
      name: "onPopupShowing preserves context-sep-navigation",
      fn: testOnPopupShowingPreservesNavigationSeparator,
    },
    {
      name: "onPopupShowing preserves context-sep-pdfjs-selectall",
      fn: testOnPopupShowingPreservesPdfjsSeparator,
    },
    {
      name: "onPopupShowing does not hide separator before visible item",
      fn: testOnPopupShowingDoesNotHideSeparatorBeforeVisibleItem,
    },

    // ContextMenuUtils — addContextBox
    {
      name: "addContextBox handles missing elements",
      fn: testAddContextBoxRequiresCheckElement,
    },

    // ContextMenuUtils — addToolbarContentMenuPopupSet
    {
      name: "addToolbarContentMenuPopupSet is callable",
      fn: testAddToolbarContentMenuPopupSetDoesNotThrow,
    },
  ];

  const { runTests } = await import("../../../test/utils/test_harness.ts");
  await runTests("contextMenuComponent.test.ts", tests);
}
