/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @colocated-env browser

/**
 * Unit tests for Offscreen API Polyfill
 *
 * These tests prioritize deterministic behavior in browser integration mode:
 * - Validation and lifecycle tests stub internal iframe load behavior
 * - Installation tests use isolated mock extension globals
 */

import {
  OffscreenPolyfill,
  installOffscreenPolyfill,
  type OffscreenAPI,
  type OffscreenDocumentOptions,
} from "./OffscreenPolyfill.sys.mts";
import {
  assert,
  assertEquals,
  assertRejects,
} from "../test-helpers/assertions.mts";

declare global {
  // Extend global Window interface
  interface Window {
    __FLOORP_OFFSCREEN_MODULE__?: boolean;
  }

  // Declare chrome namespace
  // eslint-disable-next-line no-var
  var chrome: {
    offscreen?: OffscreenAPI;
    runtime?: {
      getURL(path: string): string;
    };
    // deno-lint-ignore no-explicit-any
    [key: string]: any;
  };

  // Declare browser namespace
  // eslint-disable-next-line no-var
  var browser: {
    offscreen?: OffscreenAPI;
    runtime?: {
      getURL(path: string): string;
    };
    // deno-lint-ignore no-explicit-any
    [key: string]: any;
  };
}

function clearOffscreenFrames(): void {
  const doc = document;
  if (typeof doc === "undefined" || doc === null) {
    return;
  }

  for (const frame of doc.querySelectorAll(
    "iframe[data-floorp-offscreen='true']",
  )) {
    frame.remove();
  }
}

async function resetEnvironment(): Promise<void> {
  try {
    await chrome?.offscreen?.closeDocument?.();
  } catch {
    // best-effort cleanup
  }
  clearOffscreenFrames();
}

function createDeterministicPolyfill(): OffscreenPolyfill {
  return new OffscreenPolyfill({
    debug: true,
    createHiddenIframeForTest: (url: string): HTMLIFrameElement => {
      const doc = document;
      if (typeof doc === "undefined" || doc === null) {
        throw new Error("Document is unavailable in test context");
      }

      const iframe = doc.createElement("iframe");
      iframe.src = url;
      iframe.setAttribute("data-floorp-offscreen", "true");
      iframe.style.cssText = "display: none";
      (doc.body ?? doc.documentElement)?.appendChild(iframe);
      return iframe;
    },
    waitForIframeLoadForTest: async () => {
      // No-op for deterministic local tests.
    },
  });
}

async function withMockExtensionGlobals<T>(fn: () => Promise<T>): Promise<T> {
  const g = globalThis as unknown as Record<string, unknown>;
  const hadChrome = Object.prototype.hasOwnProperty.call(g, "chrome");
  const hadBrowser = Object.prototype.hasOwnProperty.call(g, "browser");
  const prevChrome = g.chrome;
  const prevBrowser = g.browser;

  const identityRuntime = {
    getURL(path: string): string {
      return path;
    },
  };

  const nextChrome =
    prevChrome && typeof prevChrome === "object"
      ? { ...(prevChrome as Record<string, unknown>) }
      : {};
  const nextBrowser =
    prevBrowser && typeof prevBrowser === "object"
      ? { ...(prevBrowser as Record<string, unknown>) }
      : {};

  nextChrome.runtime = identityRuntime;
  nextBrowser.runtime = identityRuntime;

  g.chrome = nextChrome;
  g.browser = nextBrowser;

  try {
    return await fn();
  } finally {
    if (hadChrome) {
      g.chrome = prevChrome;
    } else {
      delete g.chrome;
    }

    if (hadBrowser) {
      g.browser = prevBrowser;
    } else {
      delete g.browser;
    }

    await resetEnvironment();
  }
}

function defaultOptions(
  overrides: Partial<OffscreenDocumentOptions> = {},
): OffscreenDocumentOptions {
  return {
    url: "about:blank",
    reasons: ["DOM_PARSER"],
    justification: "offscreen polyfill unit test",
    ...overrides,
  };
}

// =============================================================================
// Test Suite: Basic Functionality
// =============================================================================

/**
 * Test: Should create and close offscreen document successfully
 */
async function testCreateDocument() {
  await resetEnvironment();
  const polyfill = createDeterministicPolyfill();

  assertEquals(
    await polyfill.hasDocument(),
    false,
    "Document should not exist before createDocument",
  );

  await polyfill.createDocument(defaultOptions());
  const hasDoc = await polyfill.hasDocument();
  assertEquals(hasDoc, true, "Document should exist after creation");

  await polyfill.closeDocument();
  assertEquals(
    await polyfill.hasDocument(),
    false,
    "Document should not exist after closeDocument",
  );
}

/**
 * Test: Should fail to create second document
 */
async function testSingleDocumentConstraint() {
  await resetEnvironment();
  const polyfill = createDeterministicPolyfill();

  await polyfill.createDocument(defaultOptions());

  await assertRejects(
    () => polyfill.createDocument(defaultOptions()),
    "Only one offscreen document",
    "Second createDocument should be rejected",
  );

  await polyfill.closeDocument();
}

/**
 * Test: Should close document successfully
 */
async function testCloseDocument() {
  await resetEnvironment();
  const polyfill = createDeterministicPolyfill();

  await polyfill.createDocument(defaultOptions());
  assertEquals(
    await polyfill.hasDocument(),
    true,
    "Document should exist before close",
  );

  await polyfill.closeDocument();

  const hasDoc = await polyfill.hasDocument();
  assertEquals(hasDoc, false, "Document should not exist after closure");
}

/**
 * Test: Should allow closing non-existent document without error
 */
async function testCloseNonExistentDocument() {
  await resetEnvironment();
  const polyfill = createDeterministicPolyfill();

  await polyfill.closeDocument();

  const hasDoc = await polyfill.hasDocument();
  assertEquals(hasDoc, false, "Should report no document");
}

/**
 * Test: Should check document existence correctly
 */
async function testHasDocument() {
  await resetEnvironment();
  const polyfill = createDeterministicPolyfill();

  let hasDoc = await polyfill.hasDocument();
  assertEquals(hasDoc, false, "Should have no document initially");

  await polyfill.createDocument(defaultOptions());
  hasDoc = await polyfill.hasDocument();
  assertEquals(hasDoc, true, "Should have document after creation");

  await polyfill.closeDocument();
  hasDoc = await polyfill.hasDocument();
  assertEquals(hasDoc, false, "Should have no document after closure");
}

// =============================================================================
// Test Suite: Validation
// =============================================================================

/**
 * Test: Should require at least one reason
 */
async function testRequireReason() {
  await resetEnvironment();
  const polyfill = createDeterministicPolyfill();

  await assertRejects(
    () =>
      polyfill.createDocument({
        ...defaultOptions(),
        reasons: [] as unknown as OffscreenDocumentOptions["reasons"],
      }),
    "at least one reason",
    "Should reject when reasons is empty",
  );

  assertEquals(
    await polyfill.hasDocument(),
    false,
    "Invalid createDocument should not leave a document",
  );
}

/**
 * Test: Should require URL
 */
async function testRequireURL() {
  await resetEnvironment();
  const polyfill = createDeterministicPolyfill();

  await assertRejects(
    () => polyfill.createDocument(defaultOptions({ url: "" })),
    "URL must be specified",
    "Should reject when URL is empty",
  );
}

/**
 * Test: Should require justification
 */
async function testRequireJustification() {
  await resetEnvironment();
  const polyfill = createDeterministicPolyfill();

  await assertRejects(
    () => polyfill.createDocument(defaultOptions({ justification: "" })),
    "Justification must be specified",
    "Should reject when justification is empty",
  );
}

// =============================================================================
// Test Suite: Installation
// =============================================================================

/**
 * Test: Should install polyfill into chrome object
 */
async function testInstallation() {
  await withMockExtensionGlobals(async () => {
    // @ts-ignore test reset
    delete chrome.offscreen;

    const installed = installOffscreenPolyfill({ debug: true });
    const installedOffscreen = chrome.offscreen as OffscreenAPI | undefined;

    assertEquals(installed, true, "Initial installation should succeed");
    assert(
      typeof installedOffscreen === "object",
      "chrome.offscreen should be installed",
    );
    assert(
      typeof installedOffscreen?.createDocument === "function",
      "createDocument should be a function",
    );
    assert(
      typeof installedOffscreen?.closeDocument === "function",
      "closeDocument should be a function",
    );
    assert(
      typeof installedOffscreen?.hasDocument === "function",
      "hasDocument should be a function",
    );
    assert(
      browser.offscreen === installedOffscreen,
      "browser.offscreen should mirror chrome.offscreen",
    );
  });
}

/**
 * Test: Should not reinstall if already exists
 */
async function testNoReinstall() {
  await withMockExtensionGlobals(async () => {
    installOffscreenPolyfill({ debug: true, force: true });
    const installed = installOffscreenPolyfill({ debug: true });
    assertEquals(
      installed,
      false,
      "Should not reinstall when already installed",
    );
  });
}

/**
 * Test: Should force reinstall with force option
 */
async function testForceReinstall() {
  await withMockExtensionGlobals(async () => {
    installOffscreenPolyfill({ debug: true, force: true });
    const installed = installOffscreenPolyfill({ debug: true, force: true });
    assertEquals(installed, true, "Should reinstall when force option is true");
  });
}

// =============================================================================
// Test Suite: Integration
// =============================================================================

/**
 * Test: Should work with chrome.offscreen API
 */
async function testChromeAPIIntegration() {
  await withMockExtensionGlobals(async () => {
    installOffscreenPolyfill({ debug: true, force: true });
    assert(chrome.offscreen, "chrome.offscreen should be installed");

    await chrome.offscreen!.createDocument(
      defaultOptions({
        url: "about:blank",
        reasons: ["TESTING"],
        justification: "integration test",
      }),
    );

    assertEquals(
      await chrome.offscreen!.hasDocument(),
      true,
      "Installed API should create a document",
    );

    await chrome.offscreen!.closeDocument();
    assertEquals(
      await chrome.offscreen!.hasDocument(),
      false,
      "Installed API should close the document",
    );
  });
}

/**
 * Test: Should work with multiple reasons
 */
async function testMultipleReasons() {
  await resetEnvironment();
  const polyfill = createDeterministicPolyfill();

  await polyfill.createDocument(
    defaultOptions({
      reasons: ["DOM_PARSER", "CLIPBOARD", "BLOBS"],
    }),
  );
  const hasDoc = await polyfill.hasDocument();
  assertEquals(hasDoc, true, "Should accept multiple reasons");

  await polyfill.closeDocument();
}

// =============================================================================
// Test Runner
// =============================================================================

/**
 * Run all tests
 */
async function runAllTests() {
  console.log("Running Offscreen API Polyfill Tests...\n");

  const tests: Array<{ name: string; fn: () => Promise<void> }> = [
    // Basic functionality
    { name: "Create document", fn: testCreateDocument },
    { name: "Single document constraint", fn: testSingleDocumentConstraint },
    { name: "Close document", fn: testCloseDocument },
    { name: "Close non-existent document", fn: testCloseNonExistentDocument },
    { name: "Has document", fn: testHasDocument },

    // Validation
    { name: "Require reason", fn: testRequireReason },
    { name: "Require URL", fn: testRequireURL },
    { name: "Require justification", fn: testRequireJustification },

    // Installation
    { name: "Installation", fn: testInstallation },
    { name: "No reinstall", fn: testNoReinstall },
    { name: "Force reinstall", fn: testForceReinstall },

    // Integration
    { name: "Chrome API integration", fn: testChromeAPIIntegration },
    { name: "Multiple reasons", fn: testMultipleReasons },
  ];

  let passed = 0;
  let failed = 0;
  const failureDetails: string[] = [];

  for (const test of tests) {
    try {
      await resetEnvironment();
      console.log(`Running: ${test.name}`);
      await test.fn();
      console.log(`✓ ${test.name} passed\n`);
      passed++;
    } catch (error) {
      console.error(`✗ ${test.name} failed:`, error, "\n");
      failed++;
      const message = error instanceof Error ? error.message : String(error);
      failureDetails.push(`${test.name}: ${message}`);
    } finally {
      await resetEnvironment();
    }
  }

  console.log(`\nTest Results: ${passed} passed, ${failed} failed`);

  if (failed > 0) {
    throw new Error(`Offscreen test failures: ${failureDetails.join(" | ")}`);
  }

  return true;
}

// =============================================================================
// Export for test framework integration
// =============================================================================

export {
  testCreateDocument,
  testSingleDocumentConstraint,
  testCloseDocument,
  testCloseNonExistentDocument,
  testHasDocument,
  testRequireReason,
  testRequireURL,
  testRequireJustification,
  testInstallation,
  testNoReinstall,
  testForceReinstall,
  testChromeAPIIntegration,
  testMultipleReasons,
  runAllTests,
};
