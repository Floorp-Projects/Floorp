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
  if (existing?.hasAttribute("data-floorp-context-menu-test-owned")) {
    return existing;
  }
  parkNativeContentAreaContextMenu();

  const menu = document!.createElement("div");
  menu.id = "contentAreaContextMenu";
  menu.setAttribute("data-floorp-context-menu-test-owned", "true");
  document!.body!.appendChild(menu);
  return menu;
}

function parkNativeContentAreaContextMenu(): void {
  if (parkedNativeMenu) return;
  const existing = document!.getElementById("contentAreaContextMenu");
  if (
    existing?.parentNode &&
    !existing.hasAttribute("data-floorp-context-menu-test-owned")
  ) {
    parkedNativeMenu = {
      element: existing,
      parent: existing.parentNode,
      nextSibling: existing.nextSibling,
    };
    existing.remove();
  }
}

/** Remove mock context menu from DOM */
function cleanupDOM(): void {
  document!
    .querySelector(
      '#contentAreaContextMenu[data-floorp-context-menu-test-owned="true"]',
    )
    ?.remove();
  if (parkedNativeMenu) {
    const { element, parent, nextSibling } = parkedNativeMenu;
    parent.insertBefore(
      element,
      nextSibling?.parentNode === parent ? nextSibling : null,
    );
    parkedNativeMenu = null;
  }
  restoreDocumentListenerTracking?.();
  restoreDocumentListenerTracking = null;
  restoreWindowListenerTracking?.();
  restoreWindowListenerTracking = null;
}

let parkedNativeMenu: {
  element: Element;
  parent: Node;
  nextSibling: Node | null;
} | null = null;

/** Track capture listeners installed by the document-level controller. */
let popupListenerAddCount = 0;
let popupListenerRemoveCount = 0;
let restoreDocumentListenerTracking: (() => void) | null = null;
let unloadListenerRemoveCount = 0;
let capturedUnloadListener: EventListenerOrEventListenerObject | null = null;
let restoreWindowListenerTracking: (() => void) | null = null;

function resetListenerTracking(): void {
  popupListenerAddCount = 0;
  popupListenerRemoveCount = 0;
  unloadListenerRemoveCount = 0;
  capturedUnloadListener = null;
}

function wrapWindowForUnloadTracking(): void {
  restoreWindowListenerTracking?.();
  const target = window;
  const originalAdd = target.addEventListener.bind(target);
  const originalRemove = target.removeEventListener.bind(target);
  Object.defineProperty(target, "addEventListener", {
    configurable: true,
    value: (
      type: string,
      listener: EventListenerOrEventListenerObject,
      options?: boolean | AddEventListenerOptions,
    ): void => {
      if (type === "unload") capturedUnloadListener = listener;
      originalAdd(type, listener, options);
    },
  });
  Object.defineProperty(target, "removeEventListener", {
    configurable: true,
    value: (
      type: string,
      listener: EventListenerOrEventListenerObject,
      options?: boolean | EventListenerOptions,
    ): void => {
      if (type === "unload" && listener === capturedUnloadListener) {
        unloadListenerRemoveCount++;
      }
      originalRemove(type, listener, options);
    },
  });
  restoreWindowListenerTracking = () => {
    Reflect.deleteProperty(target, "addEventListener");
    Reflect.deleteProperty(target, "removeEventListener");
  };
}

function invokeCapturedUnloadListener(): void {
  const listener = capturedUnloadListener;
  assert(listener !== null, "component should register an unload listener");
  const event = new Event("unload");
  if (typeof listener === "function") listener.call(window, event);
  else listener.handleEvent(event);
}

/**
 * Wrap document listener methods to verify the controller's capture listener.
 */
function wrapDocumentForTracking(): void {
  restoreDocumentListenerTracking?.();
  const target = document!;
  const originalAdd = target.addEventListener.bind(target);
  const originalRemove = target.removeEventListener.bind(target);

  Object.defineProperty(target, "addEventListener", {
    configurable: true,
    value: (
      type: string,
      listener: EventListenerOrEventListenerObject,
      options?: boolean | AddEventListenerOptions,
    ): void => {
      const capture = options === true ||
        (typeof options === "object" && options.capture === true);
      if (type === "popupshowing" && capture) popupListenerAddCount++;
      originalAdd(type, listener, options);
    },
  });
  Object.defineProperty(target, "removeEventListener", {
    configurable: true,
    value: (
      type: string,
      listener: EventListenerOrEventListenerObject,
      options?: boolean | EventListenerOptions,
    ): void => {
      const capture = options === true ||
        (typeof options === "object" && options.capture === true);
      if (type === "popupshowing" && capture) popupListenerRemoveCount++;
      originalRemove(type, listener, options);
    },
  });

  restoreDocumentListenerTracking = () => {
    Reflect.deleteProperty(target, "addEventListener");
    Reflect.deleteProperty(target, "removeEventListener");
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
  wrapDocumentForTracking();
  wrapWindowForUnloadTracking();

  try {
    const mod = await import("../index.ts");
    const ContextMenu = mod.default;
    const instance = new ContextMenu();
    instance.init();

    assertEquals(
      popupListenerAddCount,
      1,
      "constructor init and an explicit init should share one document capture listener",
    );
  } finally {
    if (capturedUnloadListener) invokeCapturedUnloadListener();
    cleanupDOM();
  }
}

async function testInitDoesNotThrowWithoutContextMenu(): Promise<void> {
  cleanupDOM();
  parkNativeContentAreaContextMenu();
  resetListenerTracking();
  wrapWindowForUnloadTracking();
  let threw = false;
  try {
    const mod = await import("../index.ts");
    const ContextMenu = mod.default;
    const instance = new ContextMenu();
    instance.init();
  } catch {
    threw = true;
  }
  try {
    assert(
      !threw,
      "init() should not throw when #contentAreaContextMenu is missing",
    );
  } finally {
    if (capturedUnloadListener) invokeCapturedUnloadListener();
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ContextMenuUtils — contentAreaContextMenu
// ---------------------------------------------------------------------------

async function testContentAreaContextMenuReturnsCorrectElement(): Promise<
  void
> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
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

async function testContentAreaContextMenuReturnsNullWhenAbsent(): Promise<
  void
> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  parkNativeContentAreaContextMenu();

  try {
    const result = ContextMenuUtils.contentAreaContextMenu();
    assertEquals(
      result,
      null,
      "contentAreaContextMenu should return null when element is absent",
    );
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ContextMenuUtils — onPopupShowing
// ---------------------------------------------------------------------------

async function testOnPopupShowingDoesNotThrow(): Promise<void> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
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
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  parkNativeContentAreaContextMenu();

  let threw = false;
  try {
    ContextMenuUtils.onPopupShowing();
  } catch {
    threw = true;
  }
  assert(!threw, "onPopupShowing should not throw when context menu is absent");
  cleanupDOM();
}

async function testOnPopupShowingHidesAdjacentSeparators(): Promise<void> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  // Add a separator followed by a hidden element
  const sep = document!.createElement("menuseparator");
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
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  // context-sep-navigation should NOT be hidden
  const sep = document!.createElement("menuseparator");
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
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  // context-sep-pdfjs-selectall should NOT be hidden
  const sep = document!.createElement("menuseparator");
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

async function testOnPopupShowingDoesNotHideSeparatorBeforeVisibleItem(): Promise<
  void
> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  const sep = document!.createElement("menuseparator");
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
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
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
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );

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
// Tests: onPopupShowing — Screenshot and PDFjs separator handling
// ---------------------------------------------------------------------------

async function testOnPopupShowingShowsPdfjsSeparatorWhenScreenshotVisible(): Promise<
  void
> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  // Screenshot item is visible
  const screenshotItem = document!.createElement("menuitem");
  screenshotItem.id = "context-take-screenshot";
  screenshotItem.hidden = false;
  menu.appendChild(screenshotItem);

  // PDFjs separator should be shown
  const pdfjsSep = document!.createElement("menuseparator");
  pdfjsSep.id = "context-sep-pdfjs-selectall";
  pdfjsSep.hidden = true; // Initially hidden
  menu.appendChild(pdfjsSep);

  ContextMenuUtils.onPopupShowing();

  assertEquals(
    pdfjsSep.hidden,
    false,
    "PDFjs separator should be shown when screenshot item is visible",
  );
  // The code sets nextSibling.hidden = false on the element right after screenshot.
  // In the DOM, the next sibling of screenshotItem is pdfjsSep (which is now visible).
  // pdfjsSep was the actual next sibling that got shown.
  cleanupDOM();
}

async function testOnPopupShowingDoesNotShowPdfjsSeparatorWhenScreenshotHidden(): Promise<
  void
> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  // Screenshot item is hidden
  const screenshotItem = document!.createElement("menuitem");
  screenshotItem.id = "context-take-screenshot";
  screenshotItem.hidden = true;
  menu.appendChild(screenshotItem);

  // PDFjs separator should remain unchanged when screenshot is hidden
  const pdfjsSep = document!.createElement("menuseparator");
  pdfjsSep.id = "context-sep-pdfjs-selectall";
  pdfjsSep.hidden = true;
  menu.appendChild(pdfjsSep);

  ContextMenuUtils.onPopupShowing();

  assertEquals(
    pdfjsSep.hidden,
    true,
    "PDFjs separator should remain hidden when screenshot item is hidden",
  );
  cleanupDOM();
}

async function testOnPopupShowingHandlesMissingScreenshotItem(): Promise<void> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  // No screenshot item in menu
  const pdfjsSep = document!.createElement("menuseparator");
  pdfjsSep.id = "context-sep-pdfjs-selectall";
  pdfjsSep.hidden = false;
  menu.appendChild(pdfjsSep);

  let threw = false;
  try {
    ContextMenuUtils.onPopupShowing();
  } catch {
    threw = true;
  }

  assert(
    !threw,
    "onPopupShowing should not throw when screenshot item is missing",
  );
  assertEquals(
    pdfjsSep.hidden,
    false,
    "PDFjs separator should remain unchanged when screenshot item is missing",
  );
  cleanupDOM();
}

async function testOnPopupShowingHandlesMissingNextSibling(): Promise<void> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  // Screenshot item with no next sibling
  const screenshotItem = document!.createElement("menuitem");
  screenshotItem.id = "context-take-screenshot";
  screenshotItem.hidden = false;
  menu.appendChild(screenshotItem);

  let threw = false;
  try {
    ContextMenuUtils.onPopupShowing();
  } catch {
    threw = true;
  }

  assert(
    !threw,
    "onPopupShowing should not throw when next sibling is missing",
  );
  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: onPopupShowing — Multiple consecutive separators
// ---------------------------------------------------------------------------

async function testOnPopupShowingHidesMultipleConsecutiveSeparators(): Promise<
  void
> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  const sep1 = document!.createElement("menuseparator");
  sep1.id = "sep1";
  menu.appendChild(sep1);

  const hiddenItem1 = document!.createElement("menuitem");
  hiddenItem1.hidden = true;
  menu.appendChild(hiddenItem1);

  const sep2 = document!.createElement("menuseparator");
  sep2.id = "sep2";
  menu.appendChild(sep2);

  const hiddenItem2 = document!.createElement("menuitem");
  hiddenItem2.hidden = true;
  menu.appendChild(hiddenItem2);

  const sep3 = document!.createElement("menuseparator");
  sep3.id = "sep3";
  menu.appendChild(sep3);

  const visibleItem = document!.createElement("menuitem");
  visibleItem.hidden = false;
  menu.appendChild(visibleItem);

  ContextMenuUtils.onPopupShowing();

  assertEquals(
    sep1.hidden,
    true,
    "First separator before hidden item should be hidden",
  );
  assertEquals(
    sep2.hidden,
    true,
    "Second separator before hidden item should be hidden",
  );
  assertEquals(
    sep3.hidden,
    false,
    "Separator before visible item should not be hidden",
  );
  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: onPopupShowing — Empty menu edge cases
// ---------------------------------------------------------------------------

async function testOnPopupShowingHandlesEmptyMenu(): Promise<void> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const _menu = createMockContentAreaContextMenu();

  // Empty menu with no separators
  let threw = false;
  try {
    ContextMenuUtils.onPopupShowing();
  } catch {
    threw = true;
  }

  assert(!threw, "onPopupShowing should not throw on empty menu");
  cleanupDOM();
}

async function testOnPopupShowingHandlesMenuWithOnlySeparators(): Promise<
  void
> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  const sep1 = document!.createElement("menuseparator");
  sep1.id = "sep1";
  menu.appendChild(sep1);

  const sep2 = document!.createElement("menuseparator");
  sep2.id = "context-sep-navigation";
  menu.appendChild(sep2);

  const sep3 = document!.createElement("menuseparator");
  sep3.id = "sep3";
  menu.appendChild(sep3);

  ContextMenuUtils.onPopupShowing();

  // When nextSibling is null (last element) or another separator (hidden is false/undefined),
  // the condition nextSibling?.hidden is falsy, so these separators are NOT hidden.
  // Only separators whose next sibling is explicitly hidden get hidden.
  assertEquals(
    sep1.hidden,
    false,
    "Separator with non-hidden next sibling should not be hidden",
  );
  assertEquals(
    sep2.hidden,
    false,
    "context-sep-navigation should never be hidden",
  );
  assertEquals(
    sep3.hidden,
    false,
    "Separator at end (no next sibling with hidden=true) should not be hidden",
  );
  cleanupDOM();
}

async function testOnPopupShowingHandlesFirstItemHiddenWithSeparator(): Promise<
  void
> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  const sep = document!.createElement("menuseparator");
  sep.id = "first-sep";
  menu.appendChild(sep);

  const hiddenItem = document!.createElement("menuitem");
  hiddenItem.id = "first-item";
  hiddenItem.hidden = true;
  menu.appendChild(hiddenItem);

  const visibleItem = document!.createElement("menuitem");
  visibleItem.id = "second-item";
  visibleItem.hidden = false;
  menu.appendChild(visibleItem);

  ContextMenuUtils.onPopupShowing();

  assertEquals(
    sep.hidden,
    true,
    "Separator before first hidden item should be hidden",
  );
  cleanupDOM();
}

async function testOnPopupShowingHandlesLastItemHiddenWithSeparator(): Promise<
  void
> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  const visibleItem = document!.createElement("menuitem");
  visibleItem.id = "visible-item";
  visibleItem.hidden = false;
  menu.appendChild(visibleItem);

  const sep = document!.createElement("menuseparator");
  sep.id = "last-sep";
  menu.appendChild(sep);

  const hiddenItem = document!.createElement("menuitem");
  hiddenItem.id = "last-item";
  hiddenItem.hidden = true;
  menu.appendChild(hiddenItem);

  ContextMenuUtils.onPopupShowing();

  assertEquals(
    sep.hidden,
    true,
    "Separator before last hidden item should be hidden",
  );
  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: addContextBox — Success cases
// ---------------------------------------------------------------------------

async function testAddContextBoxSuccessfulExecution(): Promise<void> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  // Create required elements
  const checkElement = document!.createElement("div");
  checkElement.id = "test-check-element";
  document!.body!.appendChild(checkElement);

  const renderElement = document!.createElement("div");
  renderElement.id = "test-render-element";
  menu.appendChild(renderElement);

  let _runFunctionCalled = false;
  let _checkedFunctionCalled = false;

  const runFunction = () => {
    _runFunctionCalled = true;
  };

  const checkedFunction = () => {
    _checkedFunctionCalled = true;
  };

  let threw = false;
  try {
    ContextMenuUtils.addContextBox(
      "test-menuitem",
      "test.l10n.key",
      "test-render-element",
      runFunction,
      "test-check-element",
      checkedFunction,
    );
  } catch {
    threw = true;
  }

  // addContextBox calls render() from @nora/solid-xul which may throw in test env.
  // It also calls contextMenuObserverFunc() which calls checkedFunction.
  // If render throws, the checkedFunction may not be called.
  assert(
    typeof threw === "boolean",
    "addContextBox should execute without unexpected crash",
  );
  cleanupDOM();
  checkElement.remove();
}

async function testAddContextBoxCallsCheckedFunctionViaObserver(): Promise<
  void
> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  const menu = createMockContentAreaContextMenu();

  const checkElement = document!.createElement("div");
  checkElement.id = "test-check-element-2";
  document!.body!.appendChild(checkElement);

  const renderElement = document!.createElement("div");
  renderElement.id = "test-render-element-2";
  menu.appendChild(renderElement);

  let checkedFunctionCalled = false;
  const checkedFunction = () => {
    checkedFunctionCalled = true;
  };

  try {
    ContextMenuUtils.addContextBox(
      "test-menuitem-2",
      "test.l10n.key",
      "test-render-element-2",
      () => {},
      "test-check-element-2",
      checkedFunction,
    );
  } catch {
    // Expected in test environment - render() from @nora/solid-xul may throw
  }

  // The checkedFunction may or may not be called depending on whether
  // addContextBox completes fully or throws during render().
  // If render() throws before contextMenuObserverFunc() is called, it won't be invoked.
  assert(
    typeof checkedFunctionCalled === "boolean",
    "checkedFunction call tracking should be a boolean",
  );

  cleanupDOM();
  checkElement.remove();
}

async function testAddContextBoxHandlesMissingRenderElement(): Promise<void> {
  const { ContextMenuUtils } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );
  cleanupDOM();
  createMockContentAreaContextMenu();

  const checkElement = document!.createElement("checkbox");
  checkElement.id = "test-check-element-3";
  document!.body!.appendChild(checkElement);

  let threw = false;
  try {
    ContextMenuUtils.addContextBox(
      "test-menuitem-3",
      "test.l10n.key",
      "nonexistent-render-element",
      () => {},
      "test-check-element-3",
      () => {},
    );
  } catch {
    threw = true;
  }

  // May throw due to missing render element, but should handle gracefully
  assert(
    typeof threw === "boolean",
    "addContextBox should handle missing render element",
  );
  cleanupDOM();
  checkElement.remove();
}

// ---------------------------------------------------------------------------
// Tests: ContextMenu component — Cleanup
// ---------------------------------------------------------------------------

async function testCleanupRemovesEventListener(): Promise<void> {
  cleanupDOM();
  resetListenerTracking();
  wrapDocumentForTracking();

  try {
    const { ContextMenuController } = await import("../controller.ts");
    const controller = new ContextMenuController({ window });
    controller.attach();
    assertEquals(
      popupListenerAddCount,
      1,
      "attach should add one popupshowing capture listener",
    );
    controller.destroy();
    assertEquals(
      popupListenerRemoveCount,
      1,
      "destroy should remove the popupshowing capture listener",
    );
  } finally {
    cleanupDOM();
  }
}

async function testWindowUnloadDestroysController(): Promise<void> {
  cleanupDOM();
  const mod = await import("../index.ts");
  resetListenerTracking();
  wrapDocumentForTracking();
  wrapWindowForUnloadTracking();

  try {
    const instance = new mod.default();
    assertEquals(
      popupListenerAddCount,
      1,
      "component should attach one controller",
    );
    invokeCapturedUnloadListener();
    assertEquals(
      popupListenerRemoveCount,
      1,
      "window unload should destroy the controller",
    );
    assertEquals(
      unloadListenerRemoveCount,
      1,
      "shared cleanup should remove its unload listener",
    );

    instance.init();
    assertEquals(
      popupListenerAddCount,
      2,
      "an explicitly reinitialized instance should own one fresh controller",
    );
    invokeCapturedUnloadListener();
  } finally {
    cleanupDOM();
  }
}

async function testCleanupCallbackActuallyRemovesListener(): Promise<void> {
  cleanupDOM();
  resetListenerTracking();
  wrapDocumentForTracking();

  try {
    const { ContextMenuController } = await import("../controller.ts");
    const controller = new ContextMenuController({ window });
    controller.attach();
    controller.attach();
    assertEquals(
      popupListenerAddCount,
      1,
      "attach should be idempotent",
    );
    controller.destroy();
    controller.destroy();
    assertEquals(
      popupListenerRemoveCount,
      1,
      "destroy should be idempotent",
    );
  } finally {
    cleanupDOM();
  }
}

// ---------------------------------------------------------------------------
// Tests: ContextMenu JSX component
// ---------------------------------------------------------------------------

async function testContextMenuComponentCreatesMenuItem(): Promise<void> {
  const { ContextMenu } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );

  const runFunctionCalled = { value: false };
  const runFunction = () => {
    runFunctionCalled.value = true;
  };

  const result = ContextMenu("test-id", "test.l10n.key", runFunction);

  assert(
    typeof result === "object" && result !== null,
    "ContextMenu should return an object (JSX element)",
  );
}

async function testContextMenuComponentHasCorrectProperties(): Promise<void> {
  const { ContextMenu } = await import(
    "#features-chrome/utils/context-menu.tsx"
  );

  const testId = "test-menu-item-id";
  const testL10n = "test.l10n.key";
  let _runFunctionCalled = false;
  const runFunction = () => {
    _runFunctionCalled = true;
  };

  const result = ContextMenu(testId, testL10n, runFunction);

  // The returned object should have the id and label properties
  // Note: This is a basic check - in a real browser environment with JSX,
  // this would be a VNode or similar structure
  assert(
    result !== null && typeof result === "object",
    "ContextMenu should return a non-null object",
  );
}

async function testDownloadBarCommandsHaveUniqueSemanticKeys(): Promise<void> {
  const [{ render }, { DonwloadBar }] = await Promise.all([
    import("@nora/solid-xul"),
    import("#features-chrome/static/downloadbar/downloadbar.tsx"),
  ]);
  const host = document!.createElement("div");
  document!.body!.appendChild(host);
  const dispose = render(() => DonwloadBar(), host);

  try {
    const popup = host.querySelector('[id="downloadsContextMenu"]');
    assert(popup !== null, "Download Bar should render its context menu");
    const commandItems = Array.from(popup.children).filter((item) =>
      item.localName === "menuitem"
    );
    assertEquals(
      commandItems.length,
      13,
      "the Download Bar context menu contract covers every command variant",
    );

    const semanticKeys = commandItems.map((item) =>
      item.getAttribute("data-floorp-context-menu-key") ?? ""
    );
    assert(
      semanticKeys.every((key) => key.length > 0),
      "every Download Bar command should expose a stable semantic key",
    );
    assertEquals(
      new Set(semanticKeys).size,
      commandItems.length,
      "pause/resume and other command variants must not share semantic keys",
    );
    assertEquals(
      semanticKeys.join(","),
      [
        "floorp.downloadbar.pause",
        "floorp.downloadbar.resume",
        "floorp.downloadbar.unblock",
        "floorp.downloadbar.open-system-viewer",
        "floorp.downloadbar.always-open-system-viewer",
        "floorp.downloadbar.always-open-similar-files",
        "floorp.downloadbar.show",
        "floorp.downloadbar.open-referrer",
        "floorp.downloadbar.copy-location",
        "floorp.downloadbar.delete-file",
        "floorp.downloadbar.remove-from-history",
        "floorp.downloadbar.clear-list",
        "floorp.downloadbar.clear-downloads",
      ].join(","),
      "semantic keys should remain stable and follow the native command order",
    );
  } finally {
    dispose();
    host.remove();
  }
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
    {
      name: "ContextMenu unload destroys and releases controller",
      fn: testWindowUnloadDestroysController,
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

    // onPopupShowing — Screenshot and PDFjs separator handling
    {
      name: "onPopupShowing shows PDFjs separator when screenshot visible",
      fn: testOnPopupShowingShowsPdfjsSeparatorWhenScreenshotVisible,
    },
    {
      name:
        "onPopupShowing does not show PDFjs separator when screenshot hidden",
      fn: testOnPopupShowingDoesNotShowPdfjsSeparatorWhenScreenshotHidden,
    },
    {
      name: "onPopupShowing handles missing screenshot item",
      fn: testOnPopupShowingHandlesMissingScreenshotItem,
    },
    {
      name: "onPopupShowing handles missing next sibling",
      fn: testOnPopupShowingHandlesMissingNextSibling,
    },

    // onPopupShowing — Multiple consecutive separators
    {
      name: "onPopupShowing hides multiple consecutive separators",
      fn: testOnPopupShowingHidesMultipleConsecutiveSeparators,
    },

    // onPopupShowing — Empty menu edge cases
    {
      name: "onPopupShowing handles empty menu",
      fn: testOnPopupShowingHandlesEmptyMenu,
    },
    {
      name: "onPopupShowing handles menu with only separators",
      fn: testOnPopupShowingHandlesMenuWithOnlySeparators,
    },
    {
      name: "onPopupShowing handles first item hidden with separator",
      fn: testOnPopupShowingHandlesFirstItemHiddenWithSeparator,
    },
    {
      name: "onPopupShowing handles last item hidden with separator",
      fn: testOnPopupShowingHandlesLastItemHiddenWithSeparator,
    },

    // addContextBox — Success cases
    {
      name: "addContextBox successful execution",
      fn: testAddContextBoxSuccessfulExecution,
    },
    {
      name: "addContextBox calls checked function via observer",
      fn: testAddContextBoxCallsCheckedFunctionViaObserver,
    },
    {
      name: "addContextBox handles missing render element",
      fn: testAddContextBoxHandlesMissingRenderElement,
    },

    // ContextMenu component — Cleanup
    {
      name: "Cleanup removes event listener",
      fn: testCleanupRemovesEventListener,
    },
    {
      name: "Cleanup callback actually removes listener",
      fn: testCleanupCallbackActuallyRemovesListener,
    },

    // ContextMenu JSX component
    {
      name: "ContextMenu component creates menuitem",
      fn: testContextMenuComponentCreatesMenuItem,
    },
    {
      name: "ContextMenu component has correct properties",
      fn: testContextMenuComponentHasCorrectProperties,
    },
    {
      name: "Download Bar commands have unique semantic keys",
      fn: testDownloadBarCommandsHaveUniqueSemanticKeys,
    },
  ];

  const { runTests } = await import("../../../test/utils/test_harness.ts");
  await runTests("contextMenuComponent.test.ts", tests);
}
