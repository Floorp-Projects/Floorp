/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Unit tests for Offscreen API Polyfill
 *
 * NOTE: These are example tests showing how the polyfill should be tested.
 * Actual test implementation depends on Floorp's testing framework.
 *
 * Status: DRAFT - Not yet integrated into test suite
 */

import {
  OffscreenPolyfill,
  installOffscreenPolyfill,
  type OffscreenAPI,
  type OffscreenDocumentOptions,
} from "./OffscreenPolyfill.sys.mts";

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

  // Declare process for Node.js/Deno compatibility checks
  // eslint-disable-next-line no-var
  // deno-lint-ignore no-explicit-any
  var process: any;
}

// =============================================================================
// Test Suite: Basic Functionality
// =============================================================================

/**
 * Test: Should create offscreen document successfully
 */
async function testCreateDocument() {
  const polyfill = new OffscreenPolyfill({ debug: true });

  const options: OffscreenDocumentOptions = {
    url: "offscreen.html",
    reasons: ["DOM_PARSER"],
    justification: "Test DOM parsing",
  };

  await polyfill.createDocument(options);
  const hasDoc = await polyfill.hasDocument();

  console.assert(hasDoc === true, "Document should exist after creation");

  // Cleanup
  await polyfill.closeDocument();
}

/**
 * Test: Should fail to create second document
 */
async function testSingleDocumentConstraint() {
  const polyfill = new OffscreenPolyfill({ debug: true });

  const options: OffscreenDocumentOptions = {
    url: "offscreen.html",
    reasons: ["DOM_PARSER"],
    justification: "Test single document constraint",
  };

  await polyfill.createDocument(options);

  let errorThrown = false;
  try {
    await polyfill.createDocument(options);
  } catch (error) {
    errorThrown = true;
    console.assert(
      (error as Error).message.includes("Only one offscreen document"),
      "Should throw error about single document constraint",
    );
  }

  console.assert(
    errorThrown,
    "Should throw error when creating second document",
  );

  // Cleanup
  await polyfill.closeDocument();
}

/**
 * Test: Should close document successfully
 */
async function testCloseDocument() {
  const polyfill = new OffscreenPolyfill({ debug: true });

  const options: OffscreenDocumentOptions = {
    url: "offscreen.html",
    reasons: ["DOM_PARSER"],
    justification: "Test document closure",
  };

  await polyfill.createDocument(options);
  await polyfill.closeDocument();

  const hasDoc = await polyfill.hasDocument();
  console.assert(hasDoc === false, "Document should not exist after closure");
}

/**
 * Test: Should allow closing non-existent document without error
 */
async function testCloseNonExistentDocument() {
  const polyfill = new OffscreenPolyfill({ debug: true });

  // Should not throw error
  await polyfill.closeDocument();

  const hasDoc = await polyfill.hasDocument();
  console.assert(hasDoc === false, "Should report no document");
}

/**
 * Test: Should check document existence correctly
 */
async function testHasDocument() {
  const polyfill = new OffscreenPolyfill({ debug: true });

  const options: OffscreenDocumentOptions = {
    url: "offscreen.html",
    reasons: ["DOM_PARSER"],
    justification: "Test document existence check",
  };

  // Initially no document
  let hasDoc = await polyfill.hasDocument();
  console.assert(hasDoc === false, "Should have no document initially");

  // After creation
  await polyfill.createDocument(options);
  hasDoc = await polyfill.hasDocument();
  console.assert(hasDoc === true, "Should have document after creation");

  // After closure
  await polyfill.closeDocument();
  hasDoc = await polyfill.hasDocument();
  console.assert(hasDoc === false, "Should have no document after closure");
}

// =============================================================================
// Test Suite: Validation
// =============================================================================

/**
 * Test: Should require at least one reason
 */
async function testRequireReason() {
  const polyfill = new OffscreenPolyfill({ debug: true });

  const options = {
    url: "offscreen.html",
    // deno-lint-ignore no-explicit-any
    reasons: [] as any,
    justification: "Test reason requirement",
  };

  let errorThrown = false;
  try {
    await polyfill.createDocument(options);
  } catch (error) {
    errorThrown = true;
    console.assert(
      (error as Error).message.includes("at least one reason"),
      "Should throw error about missing reason",
    );
  }

  console.assert(errorThrown, "Should throw error when no reasons provided");
}

/**
 * Test: Should require URL
 */
async function testRequireURL() {
  const polyfill = new OffscreenPolyfill({ debug: true });

  const options = {
    url: "",
    reasons: ["DOM_PARSER"],
    justification: "Test URL requirement",
  } as OffscreenDocumentOptions;

  let errorThrown = false;
  try {
    await polyfill.createDocument(options);
  } catch (error) {
    errorThrown = true;
    console.assert(
      (error as Error).message.includes("URL must be specified"),
      "Should throw error about missing URL",
    );
  }

  console.assert(errorThrown, "Should throw error when URL not provided");
}

/**
 * Test: Should require justification
 */
async function testRequireJustification() {
  const polyfill = new OffscreenPolyfill({ debug: true });

  const options = {
    url: "offscreen.html",
    reasons: ["DOM_PARSER"],
    justification: "",
  } as OffscreenDocumentOptions;

  let errorThrown = false;
  try {
    await polyfill.createDocument(options);
  } catch (error) {
    errorThrown = true;
    console.assert(
      (error as Error).message.includes("Justification must be specified"),
      "Should throw error about missing justification",
    );
  }

  console.assert(
    errorThrown,
    "Should throw error when justification not provided",
  );
}

// =============================================================================
// Test Suite: Installation
// =============================================================================

/**
 * Test: Should install polyfill into chrome object
 */
function testInstallation() {
  // Clear any existing installation
  // @ts-ignore: Intentionally deleting to test installation
  delete chrome.offscreen;

  const installed = installOffscreenPolyfill({ debug: true });

  console.assert(installed === true, "Should report successful installation");
  console.assert(
    typeof chrome.offscreen === "object",
    "chrome.offscreen should be an object",
  );
  console.assert(
    typeof chrome.offscreen!.createDocument === "function",
    "chrome.offscreen.createDocument should be a function",
  );
  console.assert(
    typeof chrome.offscreen!.closeDocument === "function",
    "chrome.offscreen.closeDocument should be a function",
  );
  console.assert(
    typeof chrome.offscreen!.hasDocument === "function",
    "chrome.offscreen.hasDocument should be a function",
  );
}

/**
 * Test: Should not reinstall if already exists
 */
function testNoReinstall() {
  // Install once
  installOffscreenPolyfill({ debug: true });

  // Try to install again
  const installed = installOffscreenPolyfill({ debug: true });

  console.assert(
    installed === false,
    "Should not reinstall when already exists",
  );
}

/**
 * Test: Should force reinstall with force option
 */
function testForceReinstall() {
  // Install once
  installOffscreenPolyfill({ debug: true });

  // Force reinstall
  const installed = installOffscreenPolyfill({ debug: true, force: true });

  console.assert(
    installed === true,
    "Should reinstall when force option is true",
  );
}

// =============================================================================
// Test Suite: Integration
// =============================================================================

/**
 * Test: Should work with chrome.offscreen API
 */
async function testChromeAPIIntegration() {
  // Install polyfill
  installOffscreenPolyfill({ debug: true, force: true });

  // Use via chrome.offscreen
  const options: OffscreenDocumentOptions = {
    url: "offscreen.html",
    reasons: ["DOM_PARSER"],
    justification: "Test Chrome API integration",
  };

  await chrome.offscreen!.createDocument(options);
  const hasDoc = await chrome.offscreen!.hasDocument();
  console.assert(hasDoc === true, "Should have document via chrome.offscreen");

  await chrome.offscreen!.closeDocument();
  const hasDocAfter = await chrome.offscreen!.hasDocument();
  console.assert(hasDocAfter === false, "Should not have document after close");
}

/**
 * Test: Should work with multiple reasons
 */
async function testMultipleReasons() {
  const polyfill = new OffscreenPolyfill({ debug: true });

  const options: OffscreenDocumentOptions = {
    url: "offscreen.html",
    reasons: ["DOM_PARSER", "CLIPBOARD", "BLOBS"],
    justification: "Test multiple reasons",
  };

  await polyfill.createDocument(options);
  const hasDoc = await polyfill.hasDocument();
  console.assert(hasDoc === true, "Should accept multiple reasons");

  // Cleanup
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

  const tests = [
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

  for (const test of tests) {
    try {
      console.log(`Running: ${test.name}`);
      await test.fn();
      console.log(`✓ ${test.name} passed\n`);
      passed++;
    } catch (error) {
      console.error(`✗ ${test.name} failed:`, error, "\n");
      failed++;
    }
  }

  console.log(`\nTest Results: ${passed} passed, ${failed} failed`);

  return failed === 0;
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

// Test file detection constant
const TEST_FILE_NAME = "OffscreenPolyfill.test";

// Run tests if this file is executed directly
if (
  // deno-lint-ignore no-process-global
  typeof process !== "undefined" &&
  // deno-lint-ignore no-process-global
  process.argv[1]?.includes(TEST_FILE_NAME)
) {
  runAllTests().then((success) => {
    // deno-lint-ignore no-process-global
    process.exit(success ? 0 : 1);
  });
}
