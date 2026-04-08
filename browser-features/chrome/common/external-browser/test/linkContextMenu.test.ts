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
// Runner
// ---------------------------------------------------------------------------

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
];

export async function runAllTests(): Promise<void> {
  await runTests("linkContextMenu.test.ts", tests);
}
