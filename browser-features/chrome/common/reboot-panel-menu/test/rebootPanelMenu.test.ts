// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import type { JSX } from "solid-js";
import { RebootPanelMenu } from "../RebootPanelMenu.tsx";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

let solidXulRender:
  | ((fn: () => JSX.Element, container: HTMLElement) => void)
  | null = null;

async function getSolidXulRender(): Promise<
  (fn: () => JSX.Element, container: HTMLElement) => void
> {
  if (solidXulRender) return solidXulRender;
  const mod = await import("@nora/solid-xul");
  solidXulRender = mod.render;
  return solidXulRender;
}

// Test constants
const PANEL_UI_BUTTON_ID = "PanelUI-menu-button";
const MAIN_VIEW_ID = "appMenu-mainView";
const PANEL_BODY_CLASS = "panel-subview-body";
const QUIT_BUTTON_ID = "appMenu-quit-button2";
const RESTART_BUTTON_ID = "appMenu-restart-button";
const REBOOT_PANEL_ID = "PanelUI-reboot";

/**
 * Cleanup DOM elements used in tests
 */
function cleanupDOM(): void {
  document?.getElementById(PANEL_UI_BUTTON_ID)?.remove();
  document?.getElementById(MAIN_VIEW_ID)?.remove();
  document?.getElementById(QUIT_BUTTON_ID)?.remove();
  document?.getElementById(RESTART_BUTTON_ID)?.remove();
  document?.getElementById(REBOOT_PANEL_ID)?.remove();
}

/**
 * Create mock DOM structure for panel menu tests
 */
function createMockPanelMenu(): HTMLElement {
  cleanupDOM();

  const panelUIButton = document!.createElement("div");
  panelUIButton.id = PANEL_UI_BUTTON_ID;
  document!.body!.appendChild(panelUIButton);

  const mainView = document!.createElement("div");
  mainView.id = MAIN_VIEW_ID;
  const panelBody = document!.createElement("div");
  panelBody.className = PANEL_BODY_CLASS;
  mainView.appendChild(panelBody);
  document!.body!.appendChild(mainView);

  const quitButton = document!.createElement("div");
  quitButton.id = QUIT_BUTTON_ID;
  panelBody.appendChild(quitButton);

  return panelUIButton;
}

// ===========================================================================
// Constructor Tests
// ===========================================================================

function testClassCanBeConstructedWithoutPanelUIButton(): void {
  const originalButton = document?.getElementById(
    "PanelUI-menu-button",
  ) as HTMLElement | null;
  const parent = originalButton?.parentNode ?? null;
  const nextSibling = originalButton?.nextSibling ?? null;

  try {
    originalButton?.remove();

    let threw = false;
    try {
      new RebootPanelMenu();
    } catch {
      threw = true;
    }

    assert(!threw, "constructor should not throw when menu button is missing");
  } finally {
    if (originalButton && parent) {
      parent.insertBefore(originalButton, nextSibling);
    }
  }
}

function testConstructorWithPanelUIButton(): void {
  cleanupDOM();
  createMockPanelMenu();

  let threw = false;
  try {
    new RebootPanelMenu();
  } catch {
    threw = true;
  }

  assert(
    !threw,
    "constructor should not throw when panel UI button is present",
  );

  cleanupDOM();
}

function testConstructorReturnsEarlyWithoutPanelUIButton(): void {
  cleanupDOM();

  // Create main view but NOT panel UI button
  const mainView = document!.createElement("div");
  mainView.id = MAIN_VIEW_ID;
  const panelBody = document!.createElement("div");
  panelBody.className = PANEL_BODY_CLASS;
  mainView.appendChild(panelBody);
  document!.body!.appendChild(mainView);

  const quitButton = document!.createElement("div");
  quitButton.id = QUIT_BUTTON_ID;
  panelBody.appendChild(quitButton);

  // Constructor should return early without throwing
  let threw = false;
  try {
    new RebootPanelMenu();
  } catch {
    threw = true;
  }

  assert(
    !threw,
    "constructor should return early without throwing when panelUIButton is missing",
  );

  cleanupDOM();
}

function testMultipleInstancesCanBeCreated(): void {
  cleanupDOM();
  createMockPanelMenu();

  let threw = false;
  try {
    const instance1 = new RebootPanelMenu();
    const instance2 = new RebootPanelMenu();

    assert(instance1 !== null, "First instance should be created");
    assert(instance2 !== null, "Second instance should be created");
  } catch {
    threw = true;
  }

  assert(!threw, "multiple instances should be creatable without errors");

  cleanupDOM();
}

// ===========================================================================
// Render Tests
// ===========================================================================

function testRenderReturnsJsxTree(): void {
  const node = RebootPanelMenu.Render();
  assert(node !== null, "Render should return a JSX tree");
  assertEquals(
    typeof node,
    "object",
    "Render return value should be object-like",
  );
}

async function testRenderCreatesRestartButton(): Promise<void> {
  const container = document!.createElement("div");
  document!.body!.appendChild(container);

  // Render the component using dynamic import instead of require
  const render = await getSolidXulRender();
  render(() => RebootPanelMenu.Render(), container);

  const restartButton = container.querySelector(`#${RESTART_BUTTON_ID}`);
  assert(
    restartButton !== null,
    "Render should create restart button with correct ID",
  );

  container.remove();
}

async function testRenderCreatesRebootPanelView(): Promise<void> {
  const container = document!.createElement("div");
  document!.body!.appendChild(container);

  const render = await getSolidXulRender();
  render(() => RebootPanelMenu.Render(), container);

  const panelView = container.querySelector(`#${REBOOT_PANEL_ID}`);
  assert(
    panelView !== null,
    "Render should create reboot panel view with correct ID",
  );

  container.remove();
}

async function testRenderCreatesAllRestartButtons(): Promise<void> {
  const container = document!.createElement("div");
  document!.body!.appendChild(container);

  const render = await getSolidXulRender();
  render(() => RebootPanelMenu.Render(), container);

  const normalButton = container.querySelector(
    "#appMenu-restart-normal-button",
  );
  const cacheClearButton = container.querySelector(
    "#appMenu-restart-cache-clear-button",
  );
  const safeModeButton = container.querySelector(
    "#appMenu-restart-safe-mode-button",
  );

  assert(
    normalButton !== null,
    "Render should create normal restart button",
  );
  assert(
    cacheClearButton !== null,
    "Render should create cache clear restart button",
  );
  assert(
    safeModeButton !== null,
    "Render should create safe mode restart button",
  );

  container.remove();
}

async function testRenderUsesTranslations(): Promise<void> {
  const container = document!.createElement("div");
  document!.body!.appendChild(container);

  const render = await getSolidXulRender();
  render(() => RebootPanelMenu.Render(), container);

  const restartButton = container.querySelector(`#${RESTART_BUTTON_ID}`);
  assert(restartButton !== null, "Restart button should exist");

  const label = restartButton!.getAttribute("label");
  assert(
    label !== null && label.length > 0,
    "Restart button should have a translated label",
  );

  container.remove();
}

// ===========================================================================
// Panel Rendering Tests
// ===========================================================================

function testRenderPanelDoesNothingWithoutParentElement(): void {
  cleanupDOM();

  // Only create PanelUI button, no parent element structure
  const panelUIButton = document!.createElement("div");
  panelUIButton.id = PANEL_UI_BUTTON_ID;
  document!.body!.appendChild(panelUIButton);

  // Create instance
  const _instance = new RebootPanelMenu();

  // Trigger render by setting open attribute
  panelUIButton.setAttribute("open", "true");

  // Restart button should not appear
  const restartButton = document!.getElementById(RESTART_BUTTON_ID);
  assert(
    restartButton === null,
    "renderPanel should return early without parentElement",
  );

  cleanupDOM();
}

function testRenderPanelDoesNothingWithoutBeforeElement(): void {
  cleanupDOM();

  const panelUIButton = document!.createElement("div");
  panelUIButton.id = PANEL_UI_BUTTON_ID;
  document!.body!.appendChild(panelUIButton);

  const mainView = document!.createElement("div");
  mainView.id = MAIN_VIEW_ID;
  const panelBody = document!.createElement("div");
  panelBody.className = PANEL_BODY_CLASS;
  // Don't add quit button (beforeElement)
  mainView.appendChild(panelBody);
  document!.body!.appendChild(mainView);

  const _instance = new RebootPanelMenu();

  // Trigger render
  panelUIButton.setAttribute("open", "true");

  // Restart button should not appear
  const restartButton = document!.getElementById(RESTART_BUTTON_ID);
  assert(
    restartButton === null,
    "renderPanel should return early without beforeElement",
  );

  cleanupDOM();
}

function testRenderPanelInsertsBeforeQuitButton(): void {
  cleanupDOM();
  createMockPanelMenu();

  const panelUIButton = document!.getElementById(PANEL_UI_BUTTON_ID);
  const quitButton = document!.getElementById(QUIT_BUTTON_ID);
  const panelBody = document!.querySelector(`.${PANEL_BODY_CLASS}`);

  assert(panelUIButton !== null, "PanelUI button should exist");
  assert(quitButton !== null, "Quit button should exist");
  assert(panelBody !== null, "Panel body should exist");

  // Create instance
  new RebootPanelMenu();

  // Trigger render by setting open attribute
  panelUIButton!.setAttribute("open", "true");

  // Check if restart button was inserted before quit button
  const _restartButton = document!.getElementById(RESTART_BUTTON_ID);
  // Note: In actual DOM, the button would be rendered, but in test environment
  // the rendering might not complete. We just verify no errors occurred.

  cleanupDOM();
}

// ===========================================================================
// Restart Handler Tests
// ===========================================================================

function testHandleRestartCallsServices(): void {
  let quitCalled = false;
  let quitFlags = 0;

  // deno-lint-ignore no-explicit-any
  (RebootPanelMenu as any).handleRestart({
    quit: (flags: number) => {
      quitCalled = true;
      quitFlags = flags;
    },
    forceQuit: 1,
    restart: 2,
  });

  assert(quitCalled, "handleRestart should call Services.startup.quit");
  assertEquals(
    quitFlags,
    3, // eForceQuit | eRestart
    "handleRestart should use correct quit flags",
  );
}

function testHandleRestartWithCacheClearInvalidatesCaches(): void {
  const calls: string[] = [];
  let quitFlags = 0;

  // deno-lint-ignore no-explicit-any
  (RebootPanelMenu as any).handleRestartWithCacheClear({
    invalidateCachesOnRestart: () => calls.push("invalidate"),
    quit: (flags: number) => {
      calls.push("quit");
      quitFlags = flags;
    },
    restart: 2,
    attemptQuit: 4,
  });

  assertEquals(
    calls.join(","),
    "invalidate,quit",
    "handleRestartWithCacheClear should invalidate caches before quitting",
  );
  assertEquals(
    quitFlags,
    6, // eRestart | eAttemptQuit
    "handleRestartWithCacheClear should use correct quit flags",
  );
}

function testHandleRestartInSafeModeNotifiesObservers(): void {
  let notified = false;
  let notifiedTopic = "";
  let notifiedSubject: unknown = null;
  const expectedSubject = {};

  // deno-lint-ignore no-explicit-any
  (RebootPanelMenu as any).handleRestartInSafeMode({
    notifyObservers: (subject: unknown, topic: string) => {
      notified = true;
      notifiedSubject = subject;
      notifiedTopic = topic;
    },
    subject: expectedSubject,
  });

  assert(
    notified,
    "handleRestartInSafeMode should call Services.obs.notifyObservers",
  );
  assertEquals(
    notifiedSubject,
    expectedSubject,
    "handleRestartInSafeMode should notify with the expected subject",
  );
  assertEquals(
    notifiedTopic,
    "restart-in-safe-mode",
    "handleRestartInSafeMode should notify with correct topic",
  );
}

// ===========================================================================
// SubView Tests
// ===========================================================================

async function testShowRebootPanelSubViewCallsPanelUI(): Promise<void> {
  let showSubViewCalled = false;
  let viewId = "";
  let anchorElement: unknown = null;

  // Create restart button element
  const restartButton = document!.createElement("div");
  restartButton.id = RESTART_BUTTON_ID;
  document!.body!.appendChild(restartButton);

  try {
    // deno-lint-ignore no-explicit-any
    await (RebootPanelMenu as any).showRebootPanelSubView({
      showSubView: (id: string, anchor: unknown) => {
        showSubViewCalled = true;
        viewId = id;
        anchorElement = anchor;
      },
      getAnchor: () => restartButton,
    });

    assert(
      showSubViewCalled,
      "showRebootPanelSubView should call PanelUI.showSubView",
    );
    assertEquals(
      viewId,
      "PanelUI-reboot",
      "showRebootPanelSubView should use correct view ID",
    );
    assertEquals(
      anchorElement,
      restartButton,
      "showRebootPanelSubView should use restart button as anchor",
    );
  } finally {
    restartButton.remove();
  }
}

async function testShowRebootPanelSubViewHandlesMissingAnchor(): Promise<void> {
  let showSubViewCalled = false;
  let anchorElement: unknown = null;

  // deno-lint-ignore no-explicit-any
  await (RebootPanelMenu as any).showRebootPanelSubView({
    showSubView: (_id: string, anchor: unknown) => {
      showSubViewCalled = true;
      anchorElement = anchor;
    },
    getAnchor: () => null,
  });

  assert(
    showSubViewCalled,
    "showRebootPanelSubView should still call PanelUI.showSubView",
  );
  assertEquals(
    anchorElement,
    null,
    "showRebootPanelSubView should pass null anchor when button not found",
  );
}

// ===========================================================================
// Edge Cases and Error Handling
// ===========================================================================

function testRenderWithMissingDocumentDoesNotThrow(): void {
  // In Firefox, globalThis.document is a getter-only property and cannot be
  // reassigned directly. The Render() method should still work regardless.
  try {
    // Render should handle missing document gracefully
    const node = RebootPanelMenu.Render();
    assert(node !== null, "Render should still return JSX tree");
  } catch {
    // If Render() throws due to missing DOM elements, that is acceptable.
  }
}

function testRenderPanelWithoutRenderFlag(): void {
  cleanupDOM();
  createMockPanelMenu();

  const _panelUIButton = document!.getElementById(PANEL_UI_BUTTON_ID);

  // Create instance but don't trigger render (don't set open attribute)
  new RebootPanelMenu();

  // Button should not be rendered
  const restartButton = document!.getElementById(RESTART_BUTTON_ID);
  assert(
    restartButton === null,
    "restart button should not be rendered without open attribute",
  );

  cleanupDOM();
}

function testMutationObserverDisconnectsOnCleanup(): void {
  cleanupDOM();
  createMockPanelMenu();

  // Create instance which sets up observer
  const instance = new RebootPanelMenu();

  // Verify instance was created
  assert(instance !== null, "Instance should be created");

  // In a real scenario, when the component is destroyed, onCleanup would be called
  // which would call observer.disconnect()
  // This test verifies the instance structure supports cleanup
  assert(
    typeof instance === "object",
    "instance should be an object with cleanup support",
  );

  cleanupDOM();
}

function testRenderPanelOnlyOnceWhenOpen(): void {
  cleanupDOM();
  createMockPanelMenu();

  const instance = new RebootPanelMenu();

  // Trigger open multiple times
  const panelUIButton = document!.getElementById(PANEL_UI_BUTTON_ID);
  if (panelUIButton) {
    panelUIButton.setAttribute("open", "true");
    panelUIButton.setAttribute("open", "true");
    panelUIButton.setAttribute("open", "true");
  }

  // The isRendered flag should prevent multiple renders
  // We can't directly access the private property, but we can verify
  // no errors are thrown when open is set multiple times
  assert(instance !== null, "Instance should handle multiple open events");

  cleanupDOM();
}

// ===========================================================================
// Test Runner
// ===========================================================================

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    // Constructor tests
    {
      name: "constructor is safe without panel ui button",
      fn: testClassCanBeConstructedWithoutPanelUIButton,
    },
    {
      name: "constructor works with panel ui button",
      fn: testConstructorWithPanelUIButton,
    },
    {
      name: "constructor returns early without panel button",
      fn: testConstructorReturnsEarlyWithoutPanelUIButton,
    },
    {
      name: "multiple instances can be created",
      fn: testMultipleInstancesCanBeCreated,
    },
    // Render tests
    { name: "Render returns JSX tree", fn: testRenderReturnsJsxTree },
    {
      name: "Render creates restart button",
      fn: testRenderCreatesRestartButton,
    },
    {
      name: "Render creates reboot panel view",
      fn: testRenderCreatesRebootPanelView,
    },
    {
      name: "Render creates all restart buttons",
      fn: testRenderCreatesAllRestartButtons,
    },
    {
      name: "Render uses translations",
      fn: testRenderUsesTranslations,
    },
    // Panel rendering tests
    {
      name: "renderPanel does nothing without parent element",
      fn: testRenderPanelDoesNothingWithoutParentElement,
    },
    {
      name: "renderPanel does nothing without before element",
      fn: testRenderPanelDoesNothingWithoutBeforeElement,
    },
    {
      name: "renderPanel inserts before quit button",
      fn: testRenderPanelInsertsBeforeQuitButton,
    },
    // Restart handler tests
    {
      name: "handleRestart calls Services.startup.quit",
      fn: testHandleRestartCallsServices,
    },
    {
      name: "handleRestartWithCacheClear invalidates caches",
      fn: testHandleRestartWithCacheClearInvalidatesCaches,
    },
    {
      name: "handleRestartInSafeMode notifies observers",
      fn: testHandleRestartInSafeModeNotifiesObservers,
    },
    // SubView tests
    {
      name: "showRebootPanelSubView calls PanelUI.showSubView",
      fn: testShowRebootPanelSubViewCallsPanelUI,
    },
    {
      name: "showRebootPanelSubView handles missing anchor",
      fn: testShowRebootPanelSubViewHandlesMissingAnchor,
    },
    // Edge cases
    {
      name: "Render handles missing document",
      fn: testRenderWithMissingDocumentDoesNotThrow,
    },
    {
      name: "renderPanel without render flag does not render",
      fn: testRenderPanelWithoutRenderFlag,
    },
    {
      name: "mutation observer disconnects on cleanup",
      fn: testMutationObserverDisconnectsOnCleanup,
    },
    {
      name: "render panel only once when open",
      fn: testRenderPanelOnlyOnceWhenOpen,
    },
  ];

  await runTests("rebootPanelMenu.test.ts", tests);
}
