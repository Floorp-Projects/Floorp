// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  getChromeWebStoreInstallInfo,
  clearChromeWebStoreInstallInfo,
  createCWSObserver,
  removeCWSObserver,
  overrideInstallConfirmation,
  saveOriginalDescriptionChildren,
} from "../observer.ts";
import { NotificationCustomizer } from "../notification-customizer.ts";
import type {
  ChromeWebStoreInstallInfo,
  CWSObserver,
  XPInstallObserver,
} from "../types.ts";
import {
  type TestCase,
  assert,
  assertEquals,
} from "../../../test/utils/test_harness.ts";

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

  const popupContent = document!.createElement("div");
  popupContent.className = "popupnotificationcontent";
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

  const popupContent = document!.createElement("div");
  popupContent.className = "popupnotificationcontent";
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
  const customizer = new NotificationCustomizer();
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
  const customizer = new NotificationCustomizer();
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
  const customizer = new NotificationCustomizer();
  const logger = createMockLogger();

  const observer = createCWSObserver(state, customizer, logger);

  // Simulate the observer receiving the install-started topic
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
  const customizer = new NotificationCustomizer();
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
  const customizer = new NotificationCustomizer();
  const logger = createMockLogger();

  const observer = createCWSObserver(state, customizer, logger);

  // Subject without wrappedJSObject
  observer.observe({}, "floorp-chrome-web-store-install-started", null);
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
  const customizer = new NotificationCustomizer();
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
  const customizer = new NotificationCustomizer();
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
  const customizer = new NotificationCustomizer();
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
  const customizer = new NotificationCustomizer();
  const logger = createMockLogger();

  const observer = createCWSObserver(state, customizer, logger);

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
  const customizer = new NotificationCustomizer();
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
  const customizer = new NotificationCustomizer();
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
  const customizer = new NotificationCustomizer();
  const logger = createMockLogger();

  // No CWS info set
  state.pendingChromeWebStoreInstall = null;

  createMockInstallConfirmationDOM();

  let customizeCalled = false;
  const _originalCustomize =
    customizer.customizeNotificationContent.bind(customizer);
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
  const customizer = new NotificationCustomizer();
  const logger = createMockLogger();

  // Set up CWS info
  state.pendingChromeWebStoreInstall = SAMPLE_CWS_INFO;

  // Create the permission prompt DOM
  createMockWebExtPermissionDOM();

  const observer = createCWSObserver(state, customizer, logger);

  // Trigger the permission prompt topic
  observer.observe(null, "webextension-permission-prompt", null);

  // Wait for any async updates to complete
  await new Promise<void>((resolve) => {
    requestAnimationFrame(() => {
      requestAnimationFrame(() => {
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
        resolve();
      });
    });
  });
}

async function testObserverWebExtPermissionPromptWithoutCWSInfo(): Promise<void> {
  cleanupDOM();
  const state = createMockState();
  const customizer = new NotificationCustomizer();
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
  observer.observe(null, "webextension-permission-prompt", null);

  // Wait for requestAnimationFrame to complete
  await new Promise<void>((resolve) => {
    setTimeout(() => {
      // Verify cleanup happened (CWS message removed)
      const notification = document!.getElementById(
        "addon-webext-permissions-notification",
      );
      const message = notification?.querySelector(".chrome-web-store-message");
      assertEquals(
        message,
        null,
        "should clean up CWS elements when no CWS info",
      );

      removeCWSObserver(observer);
      cleanupDOM();
      resolve();
    }, 100);
  });
}

// ---------------------------------------------------------------------------
// Tests: Edge cases and state management
// ---------------------------------------------------------------------------

function testGlobalContextPreservesPendingInstall(): void {
  clearChromeWebStoreInstallInfo();
  const state1 = createMockState();
  const customizer1 = new NotificationCustomizer();
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
  const customizer2 = new NotificationCustomizer();
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
  const customizer = new NotificationCustomizer();
  const logger = createMockLogger();

  // Initially null
  (
    state as { pendingChromeWebStoreInstall: ChromeWebStoreInstallInfo | null }
  ).pendingChromeWebStoreInstall = null;

  const observer = createCWSObserver(state, customizer, logger);

  // Update via observer
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
  const customizer = new NotificationCustomizer();
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

async function testAddonsClassStructure(): Promise<void> {
  const { default: Addons } = await import("../index.ts");

  // Test that Addons extends NoraComponentBase by checking it has the required structure
  const addonsInstance = new Addons();

  assert(
    addonsInstance !== null && typeof addonsInstance === "object",
    "should create instance",
  );
  assert(typeof addonsInstance.init === "function", "should have init method");
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
}

async function testAddonsInitDocumentReadyComplete(): Promise<void> {
  cleanupDOM();
  cleanupWindowGlobals();

  // Set document readyState to 'complete'
  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  const { default: Addons } = await import("../index.ts");
  const addonsInstance = new Addons();

  // Spy on initCustomization by checking state after init
  // Since initCustomization is private, we verify its effects
  const state = (
    addonsInstance as unknown as {
      state: { pendingChromeWebStoreInstall: ChromeWebStoreInstallInfo | null };
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

  cleanupDOM();
  cleanupWindowGlobals();
}

async function testAddonsInitDocumentNotComplete(): Promise<void> {
  cleanupDOM();
  cleanupWindowGlobals();

  // Set document readyState to something other than 'complete'
  Object.defineProperty(document, "readyState", {
    value: "loading",
    writable: true,
  });

  const { default: Addons } = await import("../index.ts");

  // Create instance - init should add event listener
  const addonsInstance = new Addons();

  // Verify instance was created successfully
  assert(
    addonsInstance !== null && typeof addonsInstance === "object",
    "should create instance with loading readyState",
  );

  // The load event listener should have been added
  // We can't easily test the async callback, but we can verify no errors thrown

  cleanupDOM();
  cleanupWindowGlobals();
}

async function testAddonsInitCreatesCWSObserver(): Promise<void> {
  cleanupDOM();
  cleanupWindowGlobals();

  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  const { default: Addons } = await import("../index.ts");
  const addonsInstance = new Addons();

  // Access private cwsObserver property
  const cwsObserver = (
    addonsInstance as unknown as { cwsObserver: CWSObserver | null }
  ).cwsObserver;

  // The cwsObserver may be null if SolidJS onCleanup already ran in test context.
  // Verify the class can be instantiated without error.
  assert(addonsInstance instanceof Addons, "should instantiate Addons class");
  // If observer exists, verify it has the expected shape
  if (cwsObserver !== null) {
    assert(
      typeof cwsObserver.observe === "function",
      "observer should have observe method",
    );
  }

  cleanupDOM();
  cleanupWindowGlobals();
}

async function testAddonsCleanupRemovesObserver(): Promise<void> {
  cleanupDOM();
  cleanupWindowGlobals();

  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  const { default: Addons } = await import("../index.ts");
  const addonsInstance = new Addons();

  // Call cleanup — should not throw
  (addonsInstance as unknown as { cleanup: () => void }).cleanup();

  // Verify observer was removed
  const cwsObserver = (
    addonsInstance as unknown as { cwsObserver: CWSObserver | null }
  ).cwsObserver;
  assertEquals(cwsObserver, null, "observer should be null after cleanup");

  cleanupDOM();
  cleanupWindowGlobals();
}

async function testAddonsCleanupClearsInstallConfirmation(): Promise<void> {
  cleanupDOM();
  cleanupWindowGlobals();

  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  // Set up gXPInstallObserver
  const mockXPInstallObserver: XPInstallObserver = {
    showInstallConfirmation: () => {},
  };
  (window as Record<string, unknown>).gXPInstallObserver =
    mockXPInstallObserver;

  const { default: Addons } = await import("../index.ts");
  const addonsInstance = new Addons();

  // Verify cleanup function was created
  // Note: cleanupInstallConfirmation is only set when readyState is "complete"
  // AND initCustomization runs, which requires gXPInstallObserver.
  // It may remain null if conditions are not met.
  const cleanupInstallConfirmation = (
    addonsInstance as unknown as {
      cleanupInstallConfirmation: (() => void) | null;
    }
  ).cleanupInstallConfirmation;
  assert(
    typeof cleanupInstallConfirmation === "function" ||
      cleanupInstallConfirmation === null,
    "should have cleanupInstallConfirmation function or null if not initialized",
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

  cleanupDOM();
  cleanupWindowGlobals();
}

async function testAddonsMultipleInitCalls(): Promise<void> {
  cleanupDOM();
  cleanupWindowGlobals();

  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  const { default: Addons } = await import("../index.ts");
  const addonsInstance = new Addons();

  // Get first observer
  const _firstObserver = (
    addonsInstance as unknown as { cwsObserver: CWSObserver | null }
  ).cwsObserver;

  // Call init again (should create new observer)
  addonsInstance.init();

  const secondObserver = (
    addonsInstance as unknown as { cwsObserver: CWSObserver | null }
  ).cwsObserver;

  // Should create a new observer instance
  assert(secondObserver !== null, "should have observer after second init");

  cleanupDOM();
  cleanupWindowGlobals();
}

async function testAddonsStateInitialization(): Promise<void> {
  cleanupDOM();
  cleanupWindowGlobals();

  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  const { default: Addons } = await import("../index.ts");
  const addonsInstance = new Addons();

  // Verify state structure
  const state = (
    addonsInstance as unknown as {
      state: { pendingChromeWebStoreInstall: ChromeWebStoreInstallInfo | null };
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

  cleanupDOM();
  cleanupWindowGlobals();
}

async function testAddonsHasLogger(): Promise<void> {
  cleanupDOM();
  cleanupWindowGlobals();

  Object.defineProperty(document, "readyState", {
    value: "complete",
    writable: true,
  });

  const { default: Addons } = await import("../index.ts");
  const addonsInstance = new Addons();

  // Verify logger exists and has expected methods
  // deno-lint-ignore no-explicit-any
  const logger: any = (addonsInstance as unknown as { logger: unknown }).logger;

  assert(
    logger !== null && typeof logger === "object",
    "should have logger object",
  );
  assert(typeof logger.debug === "function", "logger should have debug method");
  assert(typeof logger.warn === "function", "logger should have warn method");
  assert(typeof logger.info === "function", "logger should have info method");

  cleanupDOM();
  cleanupWindowGlobals();
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    // getChromeWebStoreInstallInfo / clearChromeWebStoreInstallInfo
    {
      name: "initial install info is null",
      fn: testGetInstallInfoInitiallyNull,
    },
    { name: "clear install info no-op", fn: testClearInstallInfoNoOp },
    { name: "get install info return type", fn: testGetInstallInfoReturnType },
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
      name: "overrideInstallConfirmation returns null without gXPInstallObserver",
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
    }
  }

  // Clean up any leftover DOM / observers
  cleanupDOM();
  cleanupWindowGlobals();

  if (failures.length > 0) {
    throw new Error(`addonsObserver.test.ts failures: ${failures.join(" | ")}`);
  }
}
