// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const TAB_CONTEXT_MENU_ID = "tabContextMenu";
const MENU_ID = "context_openInExternalBrowser";
const MENU_POPUP_ID = "context_openInExternalBrowser_popup";
const MARKER_ID = "context_moveTabOptions";

/** Create a mock #tabContextMenu element in the DOM */
function createMockTabContextMenu(): Element {
  cleanupDOM();

  const menu = document!.createElement("div");
  menu.id = TAB_CONTEXT_MENU_ID;

  // Add marker element as child (simulates context_moveTabOptions)
  const marker = document!.createElement("div");
  marker.id = MARKER_ID;
  menu.appendChild(marker);

  document!.body!.appendChild(menu);
  return menu;
}

/** Remove mock elements from DOM */
function cleanupDOM(): void {
  document!.getElementById(TAB_CONTEXT_MENU_ID)?.remove();
  // Also remove any menu elements that may have been created
  document!.getElementById(MENU_ID)?.remove();
  document!.getElementById(MENU_POPUP_ID)?.remove();
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Class structure
// ---------------------------------------------------------------------------

async function testTabContextMenuClassExists(): Promise<void> {
  const mod = await import("../tab-context-menu.tsx");
  assert(
    typeof mod.ExternalBrowserTabContextMenu === "function",
    "ExternalBrowserTabContextMenu should be a constructor function (class)",
  );
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Constructor & DOM injection
// ---------------------------------------------------------------------------

async function testTabContextMenuCreatesMenuElement(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    // Explicitly construct the class — import alone does NOT instantiate
    new mod.ExternalBrowserTabContextMenu();

    const menu = document!.getElementById(MENU_ID);
    assert(menu !== null, `tab context menu should create #${MENU_ID} element`);
  } finally {
    cleanupDOM();
  }
}

async function testTabContextMenuCreatesPopupElement(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    const popup = document!.getElementById(MENU_POPUP_ID);
    assert(
      popup !== null,
      `tab context menu should create #${MENU_POPUP_ID} element`,
    );
  } finally {
    cleanupDOM();
  }
}

async function testTabContextMenuDoesNotThrowWithoutParent(): Promise<void> {
  cleanupDOM();
  // No tabContextMenu element in DOM — constructor should handle gracefully

  let threw = false;
  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();
  } catch {
    threw = true;
  }
  // Constructor logs an error but should not throw
  assert(
    !threw,
    "ExternalBrowserTabContextMenu should not throw when parent element is missing",
  );
  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Menu label
// ---------------------------------------------------------------------------

async function testTabContextMenuHasLabelAttribute(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    const menu = document!.getElementById(MENU_ID);
    if (menu) {
      const label = menu.getAttribute("label");
      assert(
        label !== null && label.length > 0,
        "menu element should have a non-empty label attribute",
      );
    }
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Menu is inside tabContextMenu
// ---------------------------------------------------------------------------

async function testTabContextMenuIsChildOfTabContextMenu(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    const parent = document!.getElementById(TAB_CONTEXT_MENU_ID);
    const menu = document!.getElementById(MENU_ID);

    assert(parent !== null, "parent tabContextMenu should exist");
    assert(menu !== null, "menu should exist");

    if (parent && menu) {
      assert(
        parent.contains(menu),
        "menu element should be a descendant of tabContextMenu",
      );
    }
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Re-instantiation safety
// ---------------------------------------------------------------------------

async function testTabContextMenuMultipleInstantiationSafe(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  let threw = false;
  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();
    // Second instantiation should also succeed (may create duplicate elements)
    new mod.ExternalBrowserTabContextMenu();
  } catch {
    threw = true;
  }

  assert(!threw, "multiple constructor calls should not throw");
  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Menu element type
// ---------------------------------------------------------------------------

async function testTabContextMenuElementIsMenuType(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    const menu = document!.getElementById(MENU_ID);
    if (menu) {
      assert(menu instanceof Element, "menu should be a DOM Element");
      const tagName = menu.tagName.toLowerCase();
      assert(
        tagName.includes("menu"),
        `menu element tag should contain 'menu', got: ${tagName}`,
      );
    }
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

const tests: TestCase[] = [
  // Class structure
  {
    name: "ExternalBrowserTabContextMenu class exists",
    fn: testTabContextMenuClassExists,
  },

  // Constructor & DOM injection
  {
    name: "tab context menu creates menu element",
    fn: testTabContextMenuCreatesMenuElement,
  },
  {
    name: "tab context menu creates popup element",
    fn: testTabContextMenuCreatesPopupElement,
  },
  {
    name: "tab context menu does not throw without parent",
    fn: testTabContextMenuDoesNotThrowWithoutParent,
  },

  // Menu label
  {
    name: "tab context menu has label attribute",
    fn: testTabContextMenuHasLabelAttribute,
  },

  // DOM hierarchy
  {
    name: "tab context menu is child of tabContextMenu",
    fn: testTabContextMenuIsChildOfTabContextMenu,
  },

  // Re-instantiation safety
  {
    name: "tab context menu multiple instantiation safe",
    fn: testTabContextMenuMultipleInstantiationSafe,
  },

  // Element type
  {
    name: "tab context menu element is menu type",
    fn: testTabContextMenuElementIsMenuType,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("tabContextMenu.test.ts", tests);
}
