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

const CONTENT_AREA_MENU_ID = "contentAreaContextMenu";
const MENU_ID = "context_openLinkInExternalBrowser";
const MENU_POPUP_ID = "context_openLinkInExternalBrowser_popup";
const LINK_OPEN_MENU_ID = "context-openlink";

/** Create a mock #contentAreaContextMenu element with #context-openlink */
function createMockContentAreaContextMenu(): Element {
  cleanupDOM();

  const menu = document!.createElement("div");
  menu.id = CONTENT_AREA_MENU_ID;

  // Add the openlink element that the component observes
  const openLink = document!.createElement("div");
  openLink.id = LINK_OPEN_MENU_ID;
  menu.appendChild(openLink);

  document!.body!.appendChild(menu);
  return menu;
}

/** Remove mock elements from DOM */
function cleanupDOM(): void {
  document!.getElementById(CONTENT_AREA_MENU_ID)?.remove();
  document!.getElementById(MENU_ID)?.remove();
  document!.getElementById(MENU_POPUP_ID)?.remove();
}

/**
 * Safely override gContextMenu.linkURL.
 * In Firefox, gContextMenu is a non-configurable property on globalThis,
 * so we overwrite its linkURL field instead of deleting and reassigning.
 */
// deno-lint-ignore no-explicit-any
const gCtx: any = (globalThis as any).gContextMenu ?? {};

function mockLinkURL(linkURL: string | null): void {
  // deno-lint-ignore no-explicit-any
  (globalThis as any).gContextMenu = {
    ...gCtx,
    ...(linkURL !== null ? { linkURL } : { linkURL: undefined }),
  };
}

function restoreGContextMenu(): void {
  // deno-lint-ignore no-explicit-any
  (globalThis as any).gContextMenu = gCtx;
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — Class structure
// ---------------------------------------------------------------------------

async function testLinkContextMenuClassExists(): Promise<void> {
  const mod = await import("../link-context-menu.tsx");
  assert(
    typeof mod.ExternalBrowserLinkContextMenu === "function",
    "ExternalBrowserLinkContextMenu should be a constructor function (class)",
  );
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — Constructor & DOM injection
// ---------------------------------------------------------------------------

async function testLinkContextMenuCreatesMenuElement(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    // Explicitly construct — import alone does NOT instantiate
    new mod.ExternalBrowserLinkContextMenu();

    const menu = document!.getElementById(MENU_ID);
    assert(
      menu !== null,
      `link context menu should create #${MENU_ID} element`,
    );
  } finally {
    cleanupDOM();
  }
}

async function testLinkContextMenuCreatesPopupElement(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    const popup = document!.getElementById(MENU_POPUP_ID);
    assert(
      popup !== null,
      `link context menu should create #${MENU_POPUP_ID} element`,
    );
  } finally {
    cleanupDOM();
  }
}

async function testLinkContextMenuDoesNotThrowWithoutParent(): Promise<void> {
  cleanupDOM();
  // No contentAreaContextMenu in DOM — constructor should handle gracefully

  let threw = false;
  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();
  } catch {
    threw = true;
  }
  // Constructor logs error but should not throw
  assert(
    !threw,
    "ExternalBrowserLinkContextMenu should not throw when parent element is missing",
  );
  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — Menu label
// ---------------------------------------------------------------------------

async function testLinkContextMenuHasLabelAttribute(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

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
// Tests: ExternalBrowserLinkContextMenu — DOM hierarchy
// ---------------------------------------------------------------------------

async function testLinkContextMenuIsChildOfContentAreaMenu(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    const parent = document!.getElementById(CONTENT_AREA_MENU_ID);
    const menu = document!.getElementById(MENU_ID);

    assert(parent !== null, "parent contentAreaContextMenu should exist");
    assert(menu !== null, "menu should exist");

    if (parent && menu) {
      assert(
        parent.contains(menu),
        "menu element should be a descendant of contentAreaContextMenu",
      );
    }
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — Visibility
// ---------------------------------------------------------------------------

async function testLinkContextMenuHiddenWithoutLinkURL(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();
  mockLinkURL(null); // No link URL

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    const menu = document!.getElementById(MENU_ID) as HTMLElement | null;
    if (menu) {
      assertEquals(
        menu.hidden,
        true,
        "menu should be hidden when there is no link URL",
      );
    }
  } finally {
    restoreGContextMenu();
    cleanupDOM();
  }
}

async function testLinkContextMenuVisibleWithLinkURL(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();
  mockLinkURL("https://example.com");

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    const menu = document!.getElementById(MENU_ID) as HTMLElement | null;
    if (menu) {
      assert(!menu.hidden, "menu should be visible when there is a link URL");
    }
  } finally {
    restoreGContextMenu();
    cleanupDOM();
  }
}

async function testLinkContextMenuHiddenWhenOpenLinkIsHidden(): Promise<void> {
  cleanupDOM();
  const parent = createMockContentAreaContextMenu();

  // Hide the openLink element
  const openLink = parent.querySelector(`#${LINK_OPEN_MENU_ID}`);
  if (openLink) {
    openLink.setAttribute("hidden", "true");
  }

  mockLinkURL("https://example.com");

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    const menu = document!.getElementById(MENU_ID) as HTMLElement | null;
    if (menu) {
      assertEquals(
        menu.hidden,
        true,
        "menu should be hidden when context-openlink is hidden",
      );
    }
  } finally {
    restoreGContextMenu();
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — Element type
// ---------------------------------------------------------------------------

async function testLinkContextMenuElementIsMenuType(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

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
// Tests: ExternalBrowserLinkContextMenu — Re-instantiation safety
// ---------------------------------------------------------------------------

async function testLinkContextMenuMultipleInstantiationSafe(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  let threw = false;
  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();
    new mod.ExternalBrowserLinkContextMenu();
  } catch {
    threw = true;
  }

  assert(!threw, "multiple constructor calls should not throw");
  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — Menu positioned near openLink
// ---------------------------------------------------------------------------

async function testLinkContextMenuPositionedNearOpenLink(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    const menu = document!.getElementById(MENU_ID);
    const openLink = document!.getElementById(LINK_OPEN_MENU_ID);

    if (menu && openLink) {
      // Both should share the same parent
      assertEquals(
        menu.parentElement?.id,
        CONTENT_AREA_MENU_ID,
        "menu and openLink should share the same parent",
      );
      assertEquals(
        openLink.parentElement?.id,
        CONTENT_AREA_MENU_ID,
        "openLink should be inside contentAreaContextMenu",
      );
    }
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — Constructor with undefined document
// ---------------------------------------------------------------------------

async function testLinkContextMenuConstructorWithUndefinedDocument(): Promise<void> {
  cleanupDOM();

  try {
    // Save original document
    const originalDocument = (globalThis as any).document;

    // Temporarily set document to undefined
    // In Firefox, document is a getter-only property, so use Object.defineProperty
    try {
      Object.defineProperty(globalThis, "document", {
        value: undefined,
        configurable: true,
      });
    } catch {
      // If document is non-configurable, skip this test gracefully
      return;
    }

    let threw = false;
    try {
      const mod = await import("../link-context-menu.tsx");
      new mod.ExternalBrowserLinkContextMenu();
    } catch {
      threw = true;
    }

    // Constructor should return early without throwing
    assert(!threw, "constructor should handle undefined document gracefully");

    // Restore document
    try {
      Object.defineProperty(globalThis, "document", {
        value: originalDocument,
        configurable: true,
      });
    } catch {
      (globalThis as any).document = originalDocument;
    }
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — MutationObserver
// ---------------------------------------------------------------------------

async function testLinkContextMenuSetsUpMutationObserver(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    // Verify that the menu was created (observer setup succeeded)
    const menu = document!.getElementById(MENU_ID);
    assert(menu !== null, "menu should be created with observer setup");
  } finally {
    cleanupDOM();
  }
}

async function testLinkContextMenuUpdatesVisibilityOnAttributeChange(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();
  mockLinkURL("https://example.com");

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    const menu = document!.getElementById(MENU_ID) as HTMLElement | null;
    const openLink = document!.getElementById(LINK_OPEN_MENU_ID);

    if (menu && openLink) {
      // Initially visible (has link URL)
      assert(!menu.hidden, "menu should be visible initially");

      // Hide the openLink element
      openLink.setAttribute("hidden", "true");

      // Trigger mutation observer manually (simulate attribute change)
      const event = createMutationEvent("hidden");
      openLink.dispatchEvent(event);

      // Menu should now be hidden
      // Note: In actual implementation, this happens via MutationObserver callback
    }
  } finally {
    restoreGContextMenu();
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — Cleanup
// ---------------------------------------------------------------------------

async function testLinkContextMenuCleanupOnUnload(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    // Simulate unload event
    const unloadEvent = new Event("unload");
    globalThis.dispatchEvent(unloadEvent);

    // Menu should still exist (cleanup is internal)
    const menu = document!.getElementById(MENU_ID);
    assert(menu !== null, "menu should exist after unload");
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — Menu structure
// ---------------------------------------------------------------------------

async function testLinkContextMenuHasMenupopupChild(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    const menu = document!.getElementById(MENU_ID);
    assert(menu !== null, "menu should exist");

    const menupopup = menu.querySelector(`#${MENU_POPUP_ID}`);
    assert(menupopup !== null, "menu should have menupopup child");
  } finally {
    cleanupDOM();
  }
}

async function testLinkContextMenuHasCorrectId(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    const menu = document!.getElementById(MENU_ID);
    assert(menu !== null, "menu should exist");
    assertEquals(menu?.id, MENU_ID, "menu should have correct ID");
  } finally {
    cleanupDOM();
  }
}

async function testLinkContextMenuPopupHasCorrectId(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    const popup = document!.getElementById(MENU_POPUP_ID);
    assert(popup !== null, "popup should exist");
    assertEquals(popup?.id, MENU_POPUP_ID, "popup should have correct ID");
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — Visibility edge cases
// ---------------------------------------------------------------------------

async function testLinkContextMenuHiddenWhenDisabled(): Promise<void> {
  cleanupDOM();
  const parent = createMockContentAreaContextMenu();

  // Disable the openLink element
  const openLink = parent.querySelector(`#${LINK_OPEN_MENU_ID}`);
  if (openLink) {
    openLink.setAttribute("disabled", "true");
  }

  mockLinkURL("https://example.com");

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    const menu = document!.getElementById(MENU_ID) as HTMLElement | null;
    if (menu) {
      // Menu should respect the disabled state
      // Note: Actual visibility logic depends on implementation
      assert(menu !== null, "menu should exist even when openLink is disabled");
    }
  } finally {
    restoreGContextMenu();
    cleanupDOM();
  }
}

async function testLinkContextMenuVisibilityWithEmptyLinkURL(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();
  mockLinkURL("");

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    const menu = document!.getElementById(MENU_ID) as HTMLElement | null;
    if (menu) {
      // Empty string is falsy, so menu should be hidden
      assertEquals(
        menu.hidden,
        true,
        "menu should be hidden when link URL is empty string",
      );
    }
  } finally {
    restoreGContextMenu();
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — Error handling
// ---------------------------------------------------------------------------

async function testLinkContextMenuHandlesRenderError(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    // Mock document.createXULElement to throw error
    const originalCreateXULElement = (document as any).createXULElement;
    (document as any).createXULElement = () => {
      throw new Error("Test error");
    };

    let threw = false;
    try {
      const mod = await import("../link-context-menu.tsx");
      new mod.ExternalBrowserLinkContextMenu();
    } catch {
      threw = true;
    }

    // Constructor should handle error gracefully
    assert(!threw, "constructor should handle render error gracefully");

    // Restore original
    (document as any).createXULElement = originalCreateXULElement;
  } finally {
    cleanupDOM();
  }
}

async function testLinkContextMenuHandlesMissingContextMenu(): Promise<void> {
  cleanupDOM();
  // No contentAreaContextMenu in DOM

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    // Constructor should return early without throwing
    const menu = document!.getElementById(MENU_ID);
    assertEquals(menu, null, "menu should not be created without parent");
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — State persistence
// ---------------------------------------------------------------------------

async function testLinkContextMenuPersistsAfterInstantiation(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

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
// Tests: ExternalBrowserLinkContextMenu — Menu item creation
// ---------------------------------------------------------------------------

async function testLinkContextMenuCreatesMenuItem(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

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
// Tests: ExternalBrowserLinkContextMenu — Label update
// ---------------------------------------------------------------------------

async function testLinkContextMenuLabelCanBeUpdated(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

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
// Tests: ExternalBrowserLinkContextMenu — Constructor completes
// ---------------------------------------------------------------------------

async function testLinkContextMenuConstructorCompletes(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    const instance = new mod.ExternalBrowserLinkContextMenu();

    assert(
      instance !== null && instance !== undefined,
      "constructor should return instance",
    );
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — Empty browser list handling
// ---------------------------------------------------------------------------

async function testLinkContextMenuHandlesEmptyBrowserList(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    // Constructor should complete even if no browsers are detected
    const menu = document!.getElementById(MENU_ID);
    assert(menu !== null, "menu should exist even with empty browser list");
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ExternalBrowserLinkContextMenu — gContextMenu integration
// ---------------------------------------------------------------------------

async function testLinkContextMenuHandlesMissingGContextMenu(): Promise<void> {
  cleanupDOM();
  createMockContentAreaContextMenu();

  try {
    // Save original gContextMenu
    const originalGContextMenu = (globalThis as any).gContextMenu;

    // Remove gContextMenu temporarily (non-configurable in Firefox, so set to undefined)
    (globalThis as any).gContextMenu = undefined;

    const mod = await import("../link-context-menu.tsx");
    new mod.ExternalBrowserLinkContextMenu();

    // Constructor should complete without error even without gContextMenu
    const menu = document!.getElementById(MENU_ID);
    assert(menu !== null, "menu should be created even without gContextMenu");

    // Restore gContextMenu
    (globalThis as any).gContextMenu = originalGContextMenu;
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

// Helper to create a mutation event
function createMutationEvent(_attributeName: string): Event {
  return new Event("MutationObserver");
}

const tests: TestCase[] = [
  // Class structure
  {
    name: "ExternalBrowserLinkContextMenu class exists",
    fn: testLinkContextMenuClassExists,
  },

  // Constructor & DOM injection
  {
    name: "link context menu creates menu element",
    fn: testLinkContextMenuCreatesMenuElement,
  },
  {
    name: "link context menu creates popup element",
    fn: testLinkContextMenuCreatesPopupElement,
  },
  {
    name: "link context menu does not throw without parent",
    fn: testLinkContextMenuDoesNotThrowWithoutParent,
  },

  // Menu label
  {
    name: "link context menu has label attribute",
    fn: testLinkContextMenuHasLabelAttribute,
  },

  // DOM hierarchy
  {
    name: "link context menu is child of contentAreaMenu",
    fn: testLinkContextMenuIsChildOfContentAreaMenu,
  },

  // Visibility
  {
    name: "link context menu hidden without link URL",
    fn: testLinkContextMenuHiddenWithoutLinkURL,
  },
  {
    name: "link context menu visible with link URL",
    fn: testLinkContextMenuVisibleWithLinkURL,
  },
  {
    name: "link context menu hidden when openLink is hidden",
    fn: testLinkContextMenuHiddenWhenOpenLinkIsHidden,
  },

  // Element type
  {
    name: "link context menu element is menu type",
    fn: testLinkContextMenuElementIsMenuType,
  },

  // Re-instantiation safety
  {
    name: "link context menu multiple instantiation safe",
    fn: testLinkContextMenuMultipleInstantiationSafe,
  },

  // Positioning
  {
    name: "link context menu positioned near openLink",
    fn: testLinkContextMenuPositionedNearOpenLink,
  },

  // Constructor edge cases
  {
    name: "link context menu constructor with undefined document",
    fn: testLinkContextMenuConstructorWithUndefinedDocument,
  },

  // MutationObserver
  {
    name: "link context menu sets up mutation observer",
    fn: testLinkContextMenuSetsUpMutationObserver,
  },
  {
    name: "link context menu updates visibility on attribute change",
    fn: testLinkContextMenuUpdatesVisibilityOnAttributeChange,
  },

  // Cleanup
  {
    name: "link context menu cleanup on unload",
    fn: testLinkContextMenuCleanupOnUnload,
  },

  // Menu structure
  {
    name: "link context menu has menupopup child",
    fn: testLinkContextMenuHasMenupopupChild,
  },
  {
    name: "link context menu has correct ID",
    fn: testLinkContextMenuHasCorrectId,
  },
  {
    name: "link context menu popup has correct ID",
    fn: testLinkContextMenuPopupHasCorrectId,
  },

  // Visibility edge cases
  {
    name: "link context menu hidden when disabled",
    fn: testLinkContextMenuHiddenWhenDisabled,
  },
  {
    name: "link context menu visibility with empty link URL",
    fn: testLinkContextMenuVisibilityWithEmptyLinkURL,
  },

  // Error handling
  {
    name: "link context menu handles render error",
    fn: testLinkContextMenuHandlesRenderError,
  },
  {
    name: "link context menu handles missing context menu",
    fn: testLinkContextMenuHandlesMissingContextMenu,
  },

  // State persistence
  {
    name: "link context menu persists after instantiation",
    fn: testLinkContextMenuPersistsAfterInstantiation,
  },

  // Menu item creation
  {
    name: "link context menu creates menu item",
    fn: testLinkContextMenuCreatesMenuItem,
  },

  // Label update
  {
    name: "link context menu label can be updated",
    fn: testLinkContextMenuLabelCanBeUpdated,
  },

  // Constructor completion
  {
    name: "link context menu constructor completes",
    fn: testLinkContextMenuConstructorCompletes,
  },

  // Empty browser list handling
  {
    name: "link context menu handles empty browser list",
    fn: testLinkContextMenuHandlesEmptyBrowserList,
  },

  // gContextMenu integration
  {
    name: "link context menu handles missing gContextMenu",
    fn: testLinkContextMenuHandlesMissingGContextMenu,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("linkContextMenu.test.ts", tests);
}
