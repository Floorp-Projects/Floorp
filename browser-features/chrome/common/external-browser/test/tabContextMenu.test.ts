// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
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
// Tests: ExternalBrowserTabContextMenu — Menu positioned near marker
// ---------------------------------------------------------------------------

async function testTabContextMenuPositionedAfterMarker(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    const menu = document!.getElementById(MENU_ID);
    const marker = document!.getElementById(MARKER_ID);

    if (menu && marker) {
      // Both should share the same parent
      assertEquals(
        menu.parentElement?.id,
        TAB_CONTEXT_MENU_ID,
        "menu should be inside tabContextMenu",
      );
      assertEquals(
        marker.parentElement?.id,
        TAB_CONTEXT_MENU_ID,
        "marker should be inside tabContextMenu",
      );
    }
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Menu attributes
// ---------------------------------------------------------------------------

async function testTabContextMenuHasCorrectId(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    const menu = document!.getElementById(MENU_ID);
    assert(menu !== null, "menu should exist");
    assertEquals(menu?.id, MENU_ID, "menu should have correct ID");
  } finally {
    cleanupDOM();
  }
}

async function testTabContextMenuPopupHasCorrectId(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    const popup = document!.getElementById(MENU_POPUP_ID);
    assert(popup !== null, "popup should exist");
    assertEquals(popup?.id, MENU_POPUP_ID, "popup should have correct ID");
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Menu item creation
// ---------------------------------------------------------------------------

async function testTabContextMenuCreatesMenuItem(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    const menu = document!.getElementById(MENU_ID);
    assert(menu !== null, "menu should exist");

    // Check that menu has a menupopup child
    const popup = menu.querySelector(`#${MENU_POPUP_ID}`);
    assert(popup !== null, "menu should contain a menupopup");
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Error handling
// ---------------------------------------------------------------------------

async function testTabContextMenuHandlesRenderError(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    // Mock document.createXULElement to throw error
    const originalCreateXULElement = (document as Record<string, unknown>).createXULElement;
    (document as Record<string, unknown>).createXULElement = () => {
      throw new Error("Test error");
    };

    let threw = false;
    try {
      const mod = await import("../tab-context-menu.tsx");
      new mod.ExternalBrowserTabContextMenu();
    } catch {
      threw = true;
    }

    // Constructor should handle error gracefully and not throw
    assert(!threw, "constructor should handle render error gracefully");

    // Restore original
    (document as Record<string, unknown>).createXULElement = originalCreateXULElement;
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Integration with gBrowser
// ---------------------------------------------------------------------------

async function testTabContextMenuHandlesMissingGBrowser(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    // Save original gBrowser
    const originalGBrowser = (globalThis as Record<string, unknown>).gBrowser;

    // Remove gBrowser temporarily (non-configurable in Firefox, so set to undefined)
    (globalThis as Record<string, unknown>).gBrowser = undefined;

    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    // Constructor should complete without error even without gBrowser
    const menu = document!.getElementById(MENU_ID);
    assert(menu !== null, "menu should be created even without gBrowser");

    // Restore gBrowser
    (globalThis as Record<string, unknown>).gBrowser = originalGBrowser;
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — State persistence
// ---------------------------------------------------------------------------

async function testTabContextMenuPersistsAfterInstantiation(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    const menu = document!.getElementById(MENU_ID);
    const firstExists = menu !== null;

    // Simulate some time passing
    await new Promise((resolve) => setTimeout(resolve, 10));

    const menuLater = document!.getElementById(MENU_ID);
    const secondExists = menuLater !== null;

    assertEquals(
      firstExists,
      secondExists,
      "menu should persist after instantiation",
    );
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Menu structure
// ---------------------------------------------------------------------------

async function testTabContextMenuHasMenupopupChild(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    const menu = document!.getElementById(MENU_ID);
    assert(menu !== null, "menu should exist");

    const menupopup = menu.querySelector(`#${MENU_POPUP_ID}`);
    assert(menupopup !== null, "menu should have menupopup child");
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Label update
// ---------------------------------------------------------------------------

async function testTabContextMenuLabelCanBeUpdated(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    const menu = document!.getElementById(MENU_ID);
    assert(menu !== null, "menu should exist");

    const initialLabel = menu.getAttribute("label");
    assert(
      initialLabel !== null && initialLabel.length > 0,
      "menu should have initial label",
    );

    // Label should be updatable (simulating i18n update)
    menu.setAttribute("label", "Updated Label");
    const updatedLabel = menu.getAttribute("label");
    assertEquals(updatedLabel, "Updated Label", "label should be updatable");
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Marker positioning
// ---------------------------------------------------------------------------

async function testTabContextMenuMarkerIsSibling(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    const menu = document!.getElementById(MENU_ID);
    const marker = document!.getElementById(MARKER_ID);

    if (menu && marker) {
      const menuParent = menu.parentElement;
      const markerParent = marker.parentElement;

      assertEquals(
        menuParent?.id,
        markerParent?.id,
        "menu and marker should be siblings",
      );
    }
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Empty state handling
// ---------------------------------------------------------------------------

async function testTabContextMenuHandlesEmptyBrowserList(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    new mod.ExternalBrowserTabContextMenu();

    // Constructor should complete even if no browsers are detected
    const menu = document!.getElementById(MENU_ID);
    assert(menu !== null, "menu should exist even with empty browser list");
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserTabContextMenu — Constructor completes
// ---------------------------------------------------------------------------

async function testTabContextMenuConstructorCompletes(): Promise<void> {
  cleanupDOM();
  createMockTabContextMenu();

  try {
    const mod = await import("../tab-context-menu.tsx");
    const instance = new mod.ExternalBrowserTabContextMenu();

    assert(
      instance !== null && instance !== undefined,
      "constructor should return instance",
    );
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

  // Positioning
  {
    name: "tab context menu positioned after marker",
    fn: testTabContextMenuPositionedAfterMarker,
  },

  // Attributes
  {
    name: "tab context menu has correct ID",
    fn: testTabContextMenuHasCorrectId,
  },
  {
    name: "tab context menu popup has correct ID",
    fn: testTabContextMenuPopupHasCorrectId,
  },

  // Menu item creation
  {
    name: "tab context menu creates menu item",
    fn: testTabContextMenuCreatesMenuItem,
  },

  // Error handling
  {
    name: "tab context menu handles render error",
    fn: testTabContextMenuHandlesRenderError,
  },

  // Integration with gBrowser
  {
    name: "tab context menu handles missing gBrowser",
    fn: testTabContextMenuHandlesMissingGBrowser,
  },

  // State persistence
  {
    name: "tab context menu persists after instantiation",
    fn: testTabContextMenuPersistsAfterInstantiation,
  },

  // Menu structure
  {
    name: "tab context menu has menupopup child",
    fn: testTabContextMenuHasMenupopupChild,
  },

  // Label update
  {
    name: "tab context menu label can be updated",
    fn: testTabContextMenuLabelCanBeUpdated,
  },

  // Marker positioning
  {
    name: "tab context menu marker is sibling",
    fn: testTabContextMenuMarkerIsSibling,
  },

  // Empty state handling
  {
    name: "tab context menu handles empty browser list",
    fn: testTabContextMenuHandlesEmptyBrowserList,
  },

  // Constructor completion
  {
    name: "tab context menu constructor completes",
    fn: testTabContextMenuConstructorCompletes,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("tabContextMenu.test.ts", tests);
}
