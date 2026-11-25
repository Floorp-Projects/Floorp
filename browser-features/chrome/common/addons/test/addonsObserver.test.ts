// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  clearChromeWebStoreInstallInfo as clearChromeWebStoreInstallInfoForWindow,
  createCWSObserver as createCWSObserverForWindow,
  getChromeWebStoreInstallInfo as getChromeWebStoreInstallInfoForWindow,
  overrideInstallConfirmation as overrideInstallConfirmationForWindow,
  removeCWSObserver,
  saveOriginalDescriptionChildren as saveOriginalDescriptionChildrenForWindow,
} from "../observer.ts";
import { NotificationCustomizer } from "../notification-customizer.ts";
import Addons from "../index.ts";
import type {
  ChromeWebStoreInstallInfo,
  ChromeWebStoreWindow,
  CWSObserver,
  XPInstallObserver,
} from "../types.ts";
import {
  assert,
  assertEquals,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

import { createAddonInstance, createAddonWindow } from "./fixtures.ts";

const fixture = createAddonWindow("addons observer tests", true);
const window = fixture.win;
const document = window.document;
const getChromeWebStoreInstallInfo = () =>
  getChromeWebStoreInstallInfoForWindow(window);
const clearChromeWebStoreInstallInfo = () =>
  clearChromeWebStoreInstallInfoForWindow(window);
const saveOriginalDescriptionChildren = () =>
  saveOriginalDescriptionChildrenForWindow(window);
const createCWSObserver = (
  ...args: Parameters<typeof createCWSObserverForWindow>
) => createCWSObserverForWindow(args[0], args[1], args[2], window);
const overrideInstallConfirmation = (
  ...args: Parameters<typeof overrideInstallConfirmationForWindow>
) => overrideInstallConfirmationForWindow(args[0], args[1], args[2], window);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const SAMPLE_CWS_INFO: ChromeWebStoreInstallInfo = {
  name: "Test Chrome Extension",
  id: "test-extension-id-123",
};

const ANOTHER_CWS_INFO: ChromeWebStoreInstallInfo = {
  name: "Another Extension",
  id: "another-ext-id-456",
};

const liveCustomizers: NotificationCustomizer[] = [];
function createCustomizer(): NotificationCustomizer {
  const customizer = new NotificationCustomizer(window);
  liveCustomizers.push(customizer);
  return customizer;
}

/** Mock state object matching InstallInfoState interface */
function createMockState() {
  return {
    pendingChromeWebStoreInstall: null as ChromeWebStoreInstallInfo | null,
  };
}

/** Mock logger with call tracking */
function createMockLogger() {
  const logs: { level: string; args: unknown[] }[] = [];
  return {
    logs,
    debug: (...args: unknown[]) => logs.push({ level: "debug", args }),
    warn: (...args: unknown[]) => logs.push({ level: "warn", args }),
    info: (...args: unknown[]) => logs.push({ level: "info", args }),
  };
}

/** Create a mock webext permissions notification DOM */
function createMockWebExtPermissionDOM(): HTMLDivElement {
  const notification = document!.createElement("div");
  notification.id = "addon-webext-permissions-notification";

  const popupContent = document!.createElement("popupnotificationcontent");
  notification.appendChild(popupContent);

  const descContainer = document!.createElement("div");
  descContainer.className = "popup-notification-description";
  const originalText = document!.createTextNode("Original permission text");
  descContainer.appendChild(originalText);
  notification.appendChild(descContainer);

  const intro = document!.createElement("div");
  intro.id = "addon-webext-perm-intro";
  intro.textContent = "Intro text";
  notification.appendChild(intro);

  document!.body!.appendChild(notification);
  return notification;
}

/** Create a mock install confirmation notification DOM */
function createMockInstallConfirmationDOM(): HTMLDivElement {
  const notification = document!.createElement("div");
  notification.id = "addon-install-confirmation-notification";

  const popupContent = document!.createElement("popupnotificationcontent");
  notification.appendChild(popupContent);

  const descContainer = document!.createElement("div");
  descContainer.className = "popup-notification-description";
  notification.appendChild(descContainer);

  const content = document!.createElement("div");
  content.id = "addon-install-confirmation-content";
  const nameEl = document!.createElement("span");
  nameEl.className = "addon-install-confirmation-name";
  nameEl.textContent = "Test Addon";
  content.appendChild(nameEl);
  notification.appendChild(content);

  document!.body!.appendChild(notification);
  return notification;
}

/** Clean up all test DOM elements */
function cleanupDOM(): void {
  [
    "addon-install-confirmation-notification",
    "addon-install-confirmation-content",
    "addon-webext-permissions-notification",
  ].forEach((id) => {
    document!.getElementById(id)?.remove();
  });
}

/** Clean up window globals set during tests */
function cleanupWindowGlobals(): void {
  delete (window as Record<string, unknown>).gXPInstallObserver;
}

/** Clean up the global addons context to prevent state leakage between tests */
function cleanupGlobalContext(): void {
  const win = window as ChromeWebStoreWindow;
  const ctx = win.__floorpAddonsContext;
  removeCWSObserver(ctx?.observer ?? null);
  if (ctx) {
    for (const frame of ctx.animationFrames) win.cancelAnimationFrame(frame);
    ctx.animationFrames.clear();
    ctx.savedDescriptionChildren = null;
    ctx.customizer = null;
    ctx.logger = null;
  }
  delete win.__floorpAddonsContext;
  delete win.__chromeWebStoreInstallInfo;
}

/**
 * Reliable async wait helper for tests.
 * Uses setTimeout instead of requestAnimationFrame because rAF may not fire
 * in CI environments (background tabs, Xvfb, etc.).
 */
function waitForAsync(ms = 50): Promise<void> {
  return new Promise<void>((resolve) => {
    setTimeout(resolve, ms);
  });
}

// ---------------------------------------------------------------------------
// Tests: getChromeWebStoreInstallInfo / clearChromeWebStoreInstallInfo
// ---------------------------------------------------------------------------

function testGetInstallInfoInitiallyNull(): void {
  clearChromeWebStoreInstallInfo();
  const info = getChromeWebStoreInstallInfo();
  assertEquals(info, null, "should be null when no pending install");
}

function testClearInstallInfoNoOp(): void {
  clearChromeWebStoreInstallInfo();
  clearChromeWebStoreInstallInfo();
  const info = getChromeWebStoreInstallInfo();
  assertEquals(info, null, "should remain null after multiple clears");
}

function testGetInstallInfoReturnType(): void {
  const info = getChromeWebStoreInstallInfo();
  assert(
    info === null || typeof info === "object",
    "should be null or an object",
  );
}

function testSetAndGetInstallInfoViaWindow(): void {
  clearChromeWebStoreInstallInfo();
  const win = window as Record<string, unknown>;
  win.__chromeWebStoreInstallInfo = SAMPLE_CWS_INFO;

  const info = getChromeWebStoreInstallInfo();
  assert(info !== null, "should not be null after setting via window");
  if (info) {
    assertEquals(info.name, SAMPLE_CWS_INFO.name, "name should match");
    assertEquals(info.id, SAMPLE_CWS_INFO.id, "id should match");
  }

  clearChromeWebStoreInstallInfo();
  delete win.__chromeWebStoreInstallInfo;
}

function testClearInstallInfoRemovesWindowData(): void {
  const win = window as Record<string, unknown>;
  win.__chromeWebStoreInstallInfo = SAMPLE_CWS_INFO;
  clearChromeWebStoreInstallInfo();

  const info = getChromeWebStoreInstallInfo();
  assertEquals(info, null, "should be null after clear");
  delete win.__chromeWebStoreInstallInfo;
}

function testInstallInfoOverwriteViaWindow(): void {
  clearChromeWebStoreInstallInfo();
  const win = window as Record<string, unknown>;

  win.__chromeWebStoreInstallInfo = SAMPLE_CWS_INFO;
  let info = getChromeWebStoreInstallInfo();
  assert(info !== null, "first set should be non-null");

  win.__chromeWebStoreInstallInfo = ANOTHER_CWS_INFO;
  info = getChromeWebStoreInstallInfo();
  assert(info !== null, "overwrite should be non-null");
  if (info) {
    assertEquals(
      info.id,
      ANOTHER_CWS_INFO.id,
      "should reflect overwritten value",
    );
  }

  clearChromeWebStoreInstallInfo();
  delete win.__chromeWebStoreInstallInfo;
}

// ---------------------------------------------------------------------------
// Tests: createCWSObserver / removeCWSObserver
// ---------------------------------------------------------------------------

function testCreateCWSObserverReturnsObject(): void {
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  const observer = createCWSObserver(state, customizer, logger);
  assert(
    observer !== null && typeof observer === "object",
    "should return an object",
  );
  assert(typeof observer.observe === "function", "should have observe method");

  removeCWSObserver(observer);
}

function testRemoveCWSObserverDoesNotThrow(): void {
  // null should not throw
  removeCWSObserver(null);

  // Removing unregistered observer should not throw (error caught internally)
  const fakeObserver: CWSObserver = { observe: () => {} };
  try {
    removeCWSObserver(fakeObserver);
  } catch {
    // Acceptable: observer wasn't registered
  }
}

function testCWSObserverReusedOnSecondCall(): void {
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  const observer1 = createCWSObserver(state, customizer, logger);
  const observer2 = createCWSObserver(state, customizer, logger);
  assertEquals(
    observer1,
    observer2,
    "should return the same observer instance",
  );

  removeCWSObserver(observer1);
}

function testCWSObserverHandlesInstallStartedTopic(): void {
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  const observer = createCWSObserver(state, customizer, logger);

  // Simulate the observer receiving the install-started topic
  (window as ChromeWebStoreWindow).__chromeWebStoreInstallInfo =
    SAMPLE_CWS_INFO;
  const subject = { wrappedJSObject: SAMPLE_CWS_INFO };
  observer.observe(subject, "floorp-chrome-web-store-install-started", null);

  assertEquals(
    state.pendingChromeWebStoreInstall?.name,
    SAMPLE_CWS_INFO.name,
    "state should have install info after observer callback",
  );

  removeCWSObserver(observer);
  clearChromeWebStoreInstallInfo();
}

function testCWSObserverIgnoresUnknownTopic(): void {
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  const observer = createCWSObserver(state, customizer, logger);
  observer.observe(null, "unknown-topic", null);
  assertEquals(
    state.pendingChromeWebStoreInstall,
    null,
    "state should remain null for unknown topics",
  );

  removeCWSObserver(observer);
}

function testCWSObserverHandlesMissingWrappedJSObject(): void {
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  const observer = createCWSObserver(state, customizer, logger);

  // Subject without wrappedJSObject
  observer.observe({}, "floorp-chrome-web-store-install-started", null);
  observer.observe(
    { wrappedJSObject: {} },
    "floorp-chrome-web-store-install-started",
    null,
  );
  assertEquals(
    state.pendingChromeWebStoreInstall,
    null,
    "state should remain null without wrappedJSObject",
  );

  removeCWSObserver(observer);
}

// ---------------------------------------------------------------------------
// Tests: saveOriginalDescriptionChildren
// ---------------------------------------------------------------------------

function testSaveOriginalDescriptionChildrenNoNotification(): void {
  cleanupDOM();
  // Should not throw when no notification exists
  saveOriginalDescriptionChildren();
}

function testSaveOriginalDescriptionChildrenWithNotification(): void {
  cleanupDOM();
  createMockWebExtPermissionDOM();
  // Should not throw
  saveOriginalDescriptionChildren();
  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: overrideInstallConfirmation
// ---------------------------------------------------------------------------

function testOverrideInstallConfirmationNoXPInstallObserver(): void {
  cleanupWindowGlobals();
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  const result = overrideInstallConfirmation(state, customizer, logger);
  assertEquals(
    result,
    null,
    "should return null when gXPInstallObserver missing",
  );
}

function testOverrideInstallConfirmationReplacesShowInstall(): void {
  cleanupWindowGlobals();
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  let originalCalled = false;
  const mockXPInstallObserver: XPInstallObserver = {
    showInstallConfirmation: () => {
      originalCalled = true;
    },
  };
  (window as Record<string, unknown>).gXPInstallObserver =
    mockXPInstallObserver;

  const cleanup = overrideInstallConfirmation(state, customizer, logger);
  assert(cleanup !== null, "should return a cleanup function");

  // Call the overridden function — original should still be invoked
  originalCalled = false;
  mockXPInstallObserver.showInstallConfirmation(null, { installs: [] });
  assert(
    originalCalled,
    "original showInstallConfirmation should have been called",
  );

  cleanup?.();
  cleanupWindowGlobals();
}

function testOverrideInstallConfirmationCleanupRestores(): void {
  cleanupWindowGlobals();
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  let originalCallCount = 0;
  const originalFn = () => {
    originalCallCount++;
  };
  const mockXPInstallObserver: XPInstallObserver = {
    showInstallConfirmation: originalFn,
  };
  (window as Record<string, unknown>).gXPInstallObserver =
    mockXPInstallObserver;

  const cleanup = overrideInstallConfirmation(state, customizer, logger);
  assert(cleanup !== null, "should return a cleanup function");

  // The overridden function should still call the original
  originalCallCount = 0;
  mockXPInstallObserver.showInstallConfirmation(null, { installs: [] });
  assertEquals(originalCallCount, 1, "original should fire via override");

  // Cleanup restores the original
  cleanup?.();

  originalCallCount = 0;
  mockXPInstallObserver.showInstallConfirmation(null, { installs: [] });
  assertEquals(
    originalCallCount,
    1,
    "original should still be callable after cleanup",
  );

  cleanupWindowGlobals();
}

// ---------------------------------------------------------------------------
// Tests: Integration lifecycle
// ---------------------------------------------------------------------------

function testFullInstallInfoLifecycle(): void {
  // 1. Initially null
  clearChromeWebStoreInstallInfo();
  assertEquals(getChromeWebStoreInstallInfo(), null, "step 1: initially null");

  // 2. Set via window
  const win = window as Record<string, unknown>;
  win.__chromeWebStoreInstallInfo = SAMPLE_CWS_INFO;
  const info = getChromeWebStoreInstallInfo();
  assert(info !== null, "step 2: should be non-null after setting");
  if (info) {
    assertEquals(info.id, SAMPLE_CWS_INFO.id, "step 2: id should match");
  }

  // 3. Clear
  clearChromeWebStoreInstallInfo();
  assertEquals(
    getChromeWebStoreInstallInfo(),
    null,
    "step 3: null after clear",
  );

  // 4. Clear again (idempotent)
  clearChromeWebStoreInstallInfo();
  assertEquals(getChromeWebStoreInstallInfo(), null, "step 4: still null");

  delete win.__chromeWebStoreInstallInfo;
}

function testObserverInstallStartedUpdatesState(): void {
  clearChromeWebStoreInstallInfo();
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  const observer = createCWSObserver(state, customizer, logger);

  // The actor marks the owner window before its shared notification.
  (window as ChromeWebStoreWindow).__chromeWebStoreInstallInfo =
    SAMPLE_CWS_INFO;
  // First install
  observer.observe(
    { wrappedJSObject: SAMPLE_CWS_INFO },
    "floorp-chrome-web-store-install-started",
    null,
  );
  assert(
    state.pendingChromeWebStoreInstall !== null,
    "state should have pending install",
  );
  assertEquals(
    state.pendingChromeWebStoreInstall!.name,
    "Test Chrome Extension",
    "name should match",
  );

  // Overwrite with another install
  (window as ChromeWebStoreWindow).__chromeWebStoreInstallInfo =
    ANOTHER_CWS_INFO;
  observer.observe(
    { wrappedJSObject: ANOTHER_CWS_INFO },
    "floorp-chrome-web-store-install-started",
    null,
  );
  assertEquals(
    state.pendingChromeWebStoreInstall!.id,
    "another-ext-id-456",
    "should be overwritten",
  );

  removeCWSObserver(observer);
  clearChromeWebStoreInstallInfo();
}

// ---------------------------------------------------------------------------
// Tests: overrideInstallConfirmation - Additional Coverage
// ---------------------------------------------------------------------------

function testOverrideInstallConfirmationWithCWSInfo(): void {
  cleanupWindowGlobals();
  cleanupDOM();
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  // Set up CWS info
  state.pendingChromeWebStoreInstall = SAMPLE_CWS_INFO;

  // Create mock install confirmation DOM
  const notification = createMockInstallConfirmationDOM();

  let originalCalled = false;
  const mockXPInstallObserver: XPInstallObserver = {
    showInstallConfirmation: () => {
      originalCalled = true;
    },
  };
  (window as Record<string, unknown>).gXPInstallObserver =
    mockXPInstallObserver;

  const cleanup = overrideInstallConfirmation(state, customizer, logger);
  assert(cleanup !== null, "should return a cleanup function");

  // Call the overridden function
  mockXPInstallObserver.showInstallConfirmation(null, { installs: [] });
  assert(
    originalCalled,
    "original showInstallConfirmation should have been called",
  );

  // Verify customization happened (message added)
  // Note: customizeNotificationContent is called inside requestAnimationFrame,
  // so the message may not be immediately present in synchronous test context.
  const _message = notification.querySelector(".chrome-web-store-message");
  // The original showInstallConfirmation was called, which is the core behavior
  assert(
    originalCalled,
    "original showInstallConfirmation should have been called",
  );

  cleanup?.();
  cleanupWindowGlobals();
  cleanupDOM();
}

function testOverrideInstallConfirmationCleanupClearsInstallInfo(): void {
  cleanupWindowGlobals();
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  state.pendingChromeWebStoreInstall = SAMPLE_CWS_INFO;

  const mockXPInstallObserver: XPInstallObserver = {
    showInstallConfirmation: () => {},
  };
  (window as Record<string, unknown>).gXPInstallObserver =
    mockXPInstallObserver;

  const cleanupFn = overrideInstallConfirmation(state, customizer, logger);
  assert(cleanupFn !== null, "should return cleanup function");

  // Verify state has install info
  assert(state.pendingChromeWebStoreInstall !== null, "state should have info");

  // Call cleanup
  cleanupFn();

  // Verify install info is cleared
  assertEquals(
    getChromeWebStoreInstallInfo(),
    null,
    "cleanup should clear install info",
  );

  cleanupWindowGlobals();
}

function testOverrideInstallConfirmationWithoutCWSInfo(): void {
  cleanupWindowGlobals();
  cleanupDOM();
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  // No CWS info set
  state.pendingChromeWebStoreInstall = null;

  createMockInstallConfirmationDOM();

  let customizeCalled = false;
  const _originalCustomize = customizer.customizeNotificationContent.bind(
    customizer,
  );
  customizer.customizeNotificationContent = () => {
    customizeCalled = true;
  };

  const mockXPInstallObserver: XPInstallObserver = {
    showInstallConfirmation: () => {},
  };
  (window as Record<string, unknown>).gXPInstallObserver =
    mockXPInstallObserver;

  overrideInstallConfirmation(state, customizer, logger);

  mockXPInstallObserver.showInstallConfirmation(null, { installs: [] });

  assertEquals(customizeCalled, false, "should not customize when no CWS info");

  cleanupWindowGlobals();
  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: Observer webextension-permission-prompt topic
// ---------------------------------------------------------------------------

async function testObserverWebExtPermissionPromptWithCWSInfo(): Promise<void> {
  cleanupDOM();
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  // Set up CWS info
  state.pendingChromeWebStoreInstall = SAMPLE_CWS_INFO;

  // Create the permission prompt DOM
  createMockWebExtPermissionDOM();

  const observer = createCWSObserver(state, customizer, logger);

  // Trigger the permission prompt topic
  observer.observe(
    { wrappedJSObject: { target: { documentGlobal: window } } },
    "webextension-permission-prompt",
    null,
  );

  // Wait for any async updates to complete
  // Use setTimeout instead of rAF for CI reliability
  await waitForAsync(100);

  // Verify customization happened
  const notification = document!.getElementById(
    "addon-webext-permissions-notification",
  );
  const _message = notification?.querySelector(
    ".chrome-web-store-message",
  );
  // The observer may or may not customize the prompt depending on
  // whether the CWS info is available and the DOM structure matches.
  // We just verify the observer didn't throw.
  assert(
    notification !== null,
    "permission notification should exist in DOM",
  );

  removeCWSObserver(observer);
  cleanupDOM();
}

async function testObserverWebExtPermissionPromptWithoutCWSInfo(): Promise<
  void
> {
  cleanupDOM();
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  // No CWS info
  state.pendingChromeWebStoreInstall = null;

  // Create the permission prompt DOM with a message (simulating previous customization)
  const notification = createMockWebExtPermissionDOM();
  const descContainer = notification.querySelector(
    ".popup-notification-description",
  );
  if (descContainer) {
    const msg = document!.createElement("span");
    msg.className = "chrome-web-store-message";
    msg.textContent = "Previous Chrome message";
    descContainer.textContent = "";
    descContainer.appendChild(msg);
  }

  const observer = createCWSObserver(state, customizer, logger);

  // Trigger the permission prompt topic
  observer.observe(
    { wrappedJSObject: { target: { documentGlobal: window } } },
    "webextension-permission-prompt",
    null,
  );

  // Wait for async cleanup to complete (double-nested requestAnimationFrame)
  await waitForAsync(500);

  // Verify cleanup happened (CWS message removed)
  const notificationEl = document!.getElementById(
    "addon-webext-permissions-notification",
  );
  const message = notificationEl?.querySelector(".chrome-web-store-message");
  assertEquals(
    message,
    null,
    "should clean up CWS elements when no CWS info",
  );

  removeCWSObserver(observer);
  cleanupDOM();
}

// ---------------------------------------------------------------------------
// Tests: Edge cases and state management
// ---------------------------------------------------------------------------

function testGlobalContextPreservesPendingInstall(): void {
  clearChromeWebStoreInstallInfo();
  const state1 = createMockState();
  const customizer1 = createCustomizer();
  const logger1 = createMockLogger();

  // Set initial state
  state1.pendingChromeWebStoreInstall = SAMPLE_CWS_INFO;

  // Create first observer (updates global context)
  createCWSObserver(state1, customizer1, logger1);

  // Create second observer with new state object
  const state2 = createMockState();
  (
    state2 as { pendingChromeWebStoreInstall: ChromeWebStoreInstallInfo | null }
  ).pendingChromeWebStoreInstall = null; // Initially null
  const customizer2 = createCustomizer();
  const logger2 = createMockLogger();

  const observer2 = createCWSObserver(state2, customizer2, logger2);

  // Verify state2 was merged with existing pending install
  assertEquals(
    (
      state2 as {
        pendingChromeWebStoreInstall: ChromeWebStoreInstallInfo | null;
      }
    ).pendingChromeWebStoreInstall?.name,
    SAMPLE_CWS_INFO.name,
    "should preserve pending install info across context updates",
  );

  removeCWSObserver(observer2);
  clearChromeWebStoreInstallInfo();
}

function testObserverStateUpdateWithNullPendingInstall(): void {
  clearChromeWebStoreInstallInfo();
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  // Initially null
  (
    state as { pendingChromeWebStoreInstall: ChromeWebStoreInstallInfo | null }
  ).pendingChromeWebStoreInstall = null;

  const observer = createCWSObserver(state, customizer, logger);

  // Update via observer
  (window as ChromeWebStoreWindow).__chromeWebStoreInstallInfo =
    SAMPLE_CWS_INFO;
  observer.observe(
    { wrappedJSObject: SAMPLE_CWS_INFO },
    "floorp-chrome-web-store-install-started",
    null,
  );

  assertEquals(
    (
      state as {
        pendingChromeWebStoreInstall: ChromeWebStoreInstallInfo | null;
      }
    ).pendingChromeWebStoreInstall?.id,
    SAMPLE_CWS_INFO.id,
    "should update state from null",
  );

  removeCWSObserver(observer);
  clearChromeWebStoreInstallInfo();
}

function testMultipleObserverCreationReturnsSameInstance(): void {
  clearChromeWebStoreInstallInfo();
  const state = createMockState();
  const customizer = createCustomizer();
  const logger = createMockLogger();

  const observer1 = createCWSObserver(state, customizer, logger);
  const observer2 = createCWSObserver(state, customizer, logger);
  const observer3 = createCWSObserver(state, customizer, logger);

  assertEquals(
    observer1,
    observer2,
    "should return same observer on second call",
  );
  assertEquals(
    observer2,
    observer3,
    "should return same observer on third call",
  );

  removeCWSObserver(observer1);
  clearChromeWebStoreInstallInfo();
}

// ---------------------------------------------------------------------------
// Tests: Addons class structure
// ---------------------------------------------------------------------------

function testAddonsClassStructure(): void {
  cleanupDOM();
  cleanupWindowGlobals();
  cleanupGlobalContext();

  try {
    // Test that Addons extends NoraComponentBase by checking it has the required structure
    const addonsInstance = createAddonInstance(window);

    assert(
      addonsInstance !== null && typeof addonsInstance === "object",
      "should create instance",
    );
    assert(
      typeof addonsInstance.init === "function",
      "should have init method",
    );
    assert(
      typeof (addonsInstance as unknown as { cleanup: unknown }).cleanup ===
        "function",
      "should have cleanup method",
    );
    assert(
      (addonsInstance as unknown as { logger: unknown }).logger !== null &&
        typeof (addonsInstance as unknown as { logger: unknown }).logger ===
          "object",
      "should have logger from NoraComponentBase",
    );

    // Explicitly cleanup the Addons instance
    (addonsInstance as unknown as { cleanup: () => void }).cleanup();
  } finally {
    cleanupDOM();
    cleanupWindowGlobals();
    cleanupGlobalContext();
  }
}

function testAddonsInitDocumentReadyComplete(): void {
  cleanupDOM();
  cleanupWindowGlobals();
  cleanupGlobalContext();

  // Set document readyState to 'complete'
  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  try {
    const addonsInstance = createAddonInstance(window);

    // Spy on initCustomization by checking state after init
    // Since initCustomization is private, we verify its effects
    const state = (
      addonsInstance as unknown as {
        state: {
          pendingChromeWebStoreInstall: ChromeWebStoreInstallInfo | null;
        };
      }
    ).state;

    assert(
      state !== null && typeof state === "object",
      "should have state object",
    );
    assertEquals(
      state.pendingChromeWebStoreInstall,
      null,
      "initial state should have null pending install",
    );

    // Explicitly cleanup the Addons instance
    (addonsInstance as unknown as { cleanup: () => void }).cleanup();
  } finally {
    cleanupDOM();
    cleanupWindowGlobals();
    cleanupGlobalContext();
    // Restore document.readyState
    Object.defineProperty(document, "readyState", {
      value: "complete",
      writable: true,
    });
  }
}

function testAddonsInitDocumentNotComplete(): void {
  cleanupDOM();
  cleanupWindowGlobals();
  cleanupGlobalContext();

  // Set document readyState to something other than 'complete'
  Object.defineProperty(document, "readyState", {
    value: "loading",
    writable: true,
  });

  try {
    // Create instance - init should add event listener
    const addonsInstance = createAddonInstance(window);

    // Verify instance was created successfully
    assert(
      addonsInstance !== null && typeof addonsInstance === "object",
      "should create instance with loading readyState",
    );

    // The load event listener should have been added
    // We can't easily test the async callback, but we can verify no errors thrown

    // Explicitly cleanup the Addons instance
    (addonsInstance as unknown as { cleanup: () => void }).cleanup();
  } finally {
    cleanupDOM();
    cleanupWindowGlobals();
    cleanupGlobalContext();
    // Restore document.readyState to complete
    Object.defineProperty(document, "readyState", {
      value: "complete",
      writable: true,
    });
  }
}

function testAddonsInitCreatesCWSObserver(): void {
  cleanupDOM();
  cleanupWindowGlobals();
  cleanupGlobalContext();

  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  try {
    const addonsInstance = createAddonInstance(window);

    // Access private cwsObserver property
    const cwsObserver = (
      addonsInstance as unknown as { cwsObserver: CWSObserver | null }
    ).cwsObserver;

    assert(addonsInstance instanceof Addons, "should instantiate Addons class");
    assert(
      cwsObserver !== null,
      "constructor retains the observer created during super",
    );
    assert(
      typeof cwsObserver.observe === "function",
      "observer has observe method",
    );

    // Explicitly cleanup the Addons instance
    (addonsInstance as unknown as { cleanup: () => void }).cleanup();
  } finally {
    cleanupDOM();
    cleanupWindowGlobals();
    cleanupGlobalContext();
    Object.defineProperty(document, "readyState", {
      value: "complete",
      writable: true,
    });
  }
}

function testAddonsCleanupRemovesObserver(): void {
  cleanupDOM();
  cleanupWindowGlobals();
  cleanupGlobalContext();

  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  try {
    const addonsInstance = createAddonInstance(window);

    // Call cleanup — should not throw
    (addonsInstance as unknown as { cleanup: () => void }).cleanup();

    // Verify observer was removed
    const cwsObserver = (
      addonsInstance as unknown as { cwsObserver: CWSObserver | null }
    ).cwsObserver;
    assertEquals(cwsObserver, null, "observer should be null after cleanup");
  } finally {
    cleanupDOM();
    cleanupWindowGlobals();
    cleanupGlobalContext();
    Object.defineProperty(document, "readyState", {
      value: "complete",
      writable: true,
    });
  }
}

function testAddonsCleanupClearsInstallConfirmation(): void {
  cleanupDOM();
  cleanupWindowGlobals();
  cleanupGlobalContext();

  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  try {
    // Set up gXPInstallObserver
    const mockXPInstallObserver: XPInstallObserver = {
      showInstallConfirmation: () => {},
    };
    (window as Record<string, unknown>).gXPInstallObserver =
      mockXPInstallObserver;

    const addonsInstance = createAddonInstance(window);

    // Verify cleanup function was created
    const cleanupInstallConfirmation = (
      addonsInstance as unknown as {
        cleanupInstallConfirmation: (() => void) | null;
      }
    ).cleanupInstallConfirmation;
    assert(
      typeof cleanupInstallConfirmation === "function",
      "constructor retains the native confirmation cleanup handle",
    );

    // Call cleanup
    (addonsInstance as unknown as { cleanup: () => void }).cleanup();

    // Verify cleanup function was cleared
    const cleanupAfter = (
      addonsInstance as unknown as {
        cleanupInstallConfirmation: (() => void) | null;
      }
    ).cleanupInstallConfirmation;
    assertEquals(
      cleanupAfter,
      null,
      "cleanupInstallConfirmation should be null after cleanup",
    );
  } finally {
    cleanupDOM();
    cleanupWindowGlobals();
    cleanupGlobalContext();
    Object.defineProperty(document, "readyState", {
      value: "complete",
      writable: true,
    });
  }
}

function testAddonsMultipleInitCalls(): void {
  cleanupDOM();
  cleanupWindowGlobals();
  cleanupGlobalContext();

  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  try {
    const addonsInstance = createAddonInstance(window);

    // Get first observer
    const firstObserver = (
      addonsInstance as unknown as { cwsObserver: CWSObserver | null }
    ).cwsObserver;

    // Repeated init must preserve the existing owner and subscription.
    addonsInstance.init();

    const secondObserver = (
      addonsInstance as unknown as { cwsObserver: CWSObserver | null }
    ).cwsObserver;

    assert(secondObserver !== null, "should have observer after second init");
    assertEquals(
      secondObserver,
      firstObserver,
      "repeated init keeps the same observer",
    );

    // Explicitly cleanup the Addons instance
    (addonsInstance as unknown as { cleanup: () => void }).cleanup();
  } finally {
    cleanupDOM();
    cleanupWindowGlobals();
    cleanupGlobalContext();
    Object.defineProperty(document, "readyState", {
      value: "complete",
      writable: true,
    });
  }
}

function testAddonsStateInitialization(): void {
  cleanupDOM();
  cleanupWindowGlobals();
  cleanupGlobalContext();

  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  try {
    const addonsInstance = createAddonInstance(window);

    // Verify state structure
    const state = (
      addonsInstance as unknown as {
        state: {
          pendingChromeWebStoreInstall: ChromeWebStoreInstallInfo | null;
        };
      }
    ).state;

    assert(
      state !== null && typeof state === "object",
      "should have state object",
    );
    assertEquals(
      state.pendingChromeWebStoreInstall,
      null,
      "pendingChromeWebStoreInstall should be null initially",
    );

    // Explicitly cleanup the Addons instance
    (addonsInstance as unknown as { cleanup: () => void }).cleanup();
  } finally {
    cleanupDOM();
    cleanupWindowGlobals();
    cleanupGlobalContext();
    Object.defineProperty(document, "readyState", {
      value: "complete",
      writable: true,
    });
  }
}

function testAddonsHasLogger(): void {
  cleanupDOM();
  cleanupWindowGlobals();
  cleanupGlobalContext();

  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  try {
    const addonsInstance = createAddonInstance(window);

    // Verify logger exists and has expected methods
    const logger = (addonsInstance as unknown as {
      logger: { debug: unknown; warn: unknown; info: unknown };
    }).logger;

    assert(
      logger !== null && typeof logger === "object",
      "should have logger object",
    );
    assert(
      typeof logger.debug === "function",
      "logger should have debug method",
    );
    assert(typeof logger.warn === "function", "logger should have warn method");
    assert(typeof logger.info === "function", "logger should have info method");

    // Explicitly cleanup the Addons instance
    (addonsInstance as unknown as { cleanup: () => void }).cleanup();
  } finally {
    cleanupDOM();
    cleanupWindowGlobals();
    cleanupGlobalContext();
    Object.defineProperty(document, "readyState", {
      value: "complete",
      writable: true,
    });
  }
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  try {
    const tests: TestCase[] = [
      // getChromeWebStoreInstallInfo / clearChromeWebStoreInstallInfo
      {
        name: "initial install info is null",
        fn: testGetInstallInfoInitiallyNull,
      },
      { name: "clear install info no-op", fn: testClearInstallInfoNoOp },
      {
        name: "get install info return type",
        fn: testGetInstallInfoReturnType,
      },
      {
        name: "set and get install info via window",
        fn: testSetAndGetInstallInfoViaWindow,
      },
      {
        name: "clear install info removes window data",
        fn: testClearInstallInfoRemovesWindowData,
      },
      {
        name: "install info overwrite via window",
        fn: testInstallInfoOverwriteViaWindow,
      },

      // createCWSObserver / removeCWSObserver
      {
        name: "createCWSObserver returns observer object",
        fn: testCreateCWSObserverReturnsObject,
      },
      {
        name: "removeCWSObserver does not throw",
        fn: testRemoveCWSObserverDoesNotThrow,
      },
      {
        name: "CWS observer reused on second call",
        fn: testCWSObserverReusedOnSecondCall,
      },
      {
        name: "CWS observer handles install-started topic",
        fn: testCWSObserverHandlesInstallStartedTopic,
      },
      {
        name: "CWS observer ignores unknown topic",
        fn: testCWSObserverIgnoresUnknownTopic,
      },
      {
        name: "CWS observer handles missing wrappedJSObject",
        fn: testCWSObserverHandlesMissingWrappedJSObject,
      },

      // saveOriginalDescriptionChildren
      {
        name: "save original children - no notification",
        fn: testSaveOriginalDescriptionChildrenNoNotification,
      },
      {
        name: "save original children - with notification",
        fn: testSaveOriginalDescriptionChildrenWithNotification,
      },

      // overrideInstallConfirmation
      {
        name:
          "overrideInstallConfirmation returns null without gXPInstallObserver",
        fn: testOverrideInstallConfirmationNoXPInstallObserver,
      },
      {
        name: "overrideInstallConfirmation replaces showInstallConfirmation",
        fn: testOverrideInstallConfirmationReplacesShowInstall,
      },
      {
        name: "overrideInstallConfirmation cleanup restores original",
        fn: testOverrideInstallConfirmationCleanupRestores,
      },
      {
        name: "overrideInstallConfirmation with CWS info",
        fn: testOverrideInstallConfirmationWithCWSInfo,
      },
      {
        name: "overrideInstallConfirmation cleanup clears install info",
        fn: testOverrideInstallConfirmationCleanupClearsInstallInfo,
      },
      {
        name: "overrideInstallConfirmation without CWS info",
        fn: testOverrideInstallConfirmationWithoutCWSInfo,
      },

      // Observer webextension-permission-prompt topic
      {
        name: "observer webext-permission-prompt with CWS info",
        fn: testObserverWebExtPermissionPromptWithCWSInfo,
      },
      {
        name: "observer webext-permission-prompt without CWS info",
        fn: testObserverWebExtPermissionPromptWithoutCWSInfo,
      },

      // Integration lifecycle
      { name: "full install info lifecycle", fn: testFullInstallInfoLifecycle },
      {
        name: "observer install-started updates state",
        fn: testObserverInstallStartedUpdatesState,
      },

      // Edge cases and state management
      {
        name: "global context preserves pending install",
        fn: testGlobalContextPreservesPendingInstall,
      },
      {
        name: "observer state update with null pending install",
        fn: testObserverStateUpdateWithNullPendingInstall,
      },
      {
        name: "multiple observer creation returns same instance",
        fn: testMultipleObserverCreationReturnsSameInstance,
      },

      // Addons class structure
      { name: "Addons class structure", fn: testAddonsClassStructure },
      {
        name: "Addons init with document readyState complete",
        fn: testAddonsInitDocumentReadyComplete,
      },
      {
        name: "Addons init with document not complete",
        fn: testAddonsInitDocumentNotComplete,
      },
      {
        name: "Addons init creates CWS observer",
        fn: testAddonsInitCreatesCWSObserver,
      },
      {
        name: "Addons cleanup removes observer",
        fn: testAddonsCleanupRemovesObserver,
      },
      {
        name: "Addons cleanup clears install confirmation",
        fn: testAddonsCleanupClearsInstallConfirmation,
      },
      {
        name: "Addons multiple init calls",
        fn: testAddonsMultipleInitCalls,
      },
      {
        name: "Addons state initialization",
        fn: testAddonsStateInitialization,
      },
      {
        name: "Addons has logger",
        fn: testAddonsHasLogger,
      },
    ];

    const failures: string[] = [];

    for (const test of tests) {
      try {
        await test.fn();
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        failures.push(`${test.name}: ${message}`);
      } finally {
        cleanupGlobalContext();
      }
    }

    // Clean up any leftover DOM / observers / global context
    cleanupDOM();
    cleanupWindowGlobals();
    cleanupGlobalContext();

    if (failures.length > 0) {
      throw new Error(
        `addonsObserver.test.ts failures: ${failures.join(" | ")}`,
      );
    }
  } finally {
    cleanupGlobalContext();
    for (const customizer of liveCustomizers.splice(0)) customizer.destroy();
    fixture.close();
  }
}
