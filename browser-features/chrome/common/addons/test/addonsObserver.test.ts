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

/** Clean up all test DOM elements */
function cleanupDOM(): void {
  [
    "addon-install-confirmation-notification",
    "addon-webext-confirmation-content",
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

    // Integration lifecycle
    { name: "full install info lifecycle", fn: testFullInstallInfoLifecycle },
    {
      name: "observer install-started updates state",
      fn: testObserverInstallStartedUpdatesState,
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
