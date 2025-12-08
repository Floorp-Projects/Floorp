/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Unit tests for DocumentId API Polyfill
 *
 * Tests the documentId generation, parsing, and API wrapping functionality.
 */

import {
  DocumentIdPolyfill,
  wrapScriptingAPI,
  wrapTabsAPI,
} from "./DocumentIdPolyfill.sys.mts";

// =============================================================================
// Test Suite: DocumentId Generation and Parsing
// =============================================================================

/**
 * Test: Should generate valid documentId
 */
function testGenerateDocumentId() {
  const polyfill = new DocumentIdPolyfill({ debug: true });

  const documentId = polyfill.generateDocumentId(123, 0);

  console.assert(
    documentId.startsWith("floorp-doc-"),
    "DocumentId should start with floorp-doc-",
  );
  console.assert(
    documentId.includes("123"),
    "DocumentId should contain tabId",
  );

  console.log("✓ testGenerateDocumentId passed");
}

/**
 * Test: Should parse valid documentId
 */
function testParseDocumentId() {
  const polyfill = new DocumentIdPolyfill({ debug: true });

  const documentId = polyfill.generateDocumentId(456, 2);
  const parsed = polyfill.parseDocumentId(documentId);

  console.assert(parsed !== null, "Parsed result should not be null");
  console.assert(parsed?.tabId === 456, "TabId should be 456");
  console.assert(parsed?.frameId === 2, "FrameId should be 2");

  console.log("✓ testParseDocumentId passed");
}

/**
 * Test: Should return null for invalid documentId
 */
function testParseInvalidDocumentId() {
  const polyfill = new DocumentIdPolyfill({ debug: true });

  const parsed1 = polyfill.parseDocumentId("invalid-id");
  console.assert(parsed1 === null, "Should return null for invalid prefix");

  const parsed2 = polyfill.parseDocumentId("floorp-doc-abc-def");
  console.assert(parsed2 === null, "Should return null for non-numeric values");

  console.log("✓ testParseInvalidDocumentId passed");
}

/**
 * Test: Should validate documentId correctly
 */
function testIsValidDocumentId() {
  const polyfill = new DocumentIdPolyfill({ debug: true });

  const documentId = polyfill.generateDocumentId(789, 1);

  console.assert(
    polyfill.isValidDocumentId(documentId) === true,
    "Generated documentId should be valid",
  );
  console.assert(
    polyfill.isValidDocumentId("invalid") === false,
    "Invalid documentId should not be valid",
  );

  console.log("✓ testIsValidDocumentId passed");
}

/**
 * Test: Should convert documentIds to targets
 */
function testDocumentIdsToTargets() {
  const polyfill = new DocumentIdPolyfill({ debug: true });

  const docId1 = polyfill.generateDocumentId(100, 0);
  const docId2 = polyfill.generateDocumentId(200, 1);

  const targets = polyfill.documentIdsToTargets([docId1, docId2, "invalid"]);

  console.assert(targets.length === 2, "Should have 2 valid targets");
  console.assert(targets[0].tabId === 100, "First target tabId should be 100");
  console.assert(targets[0].frameId === 0, "First target frameId should be 0");
  console.assert(targets[1].tabId === 200, "Second target tabId should be 200");
  console.assert(targets[1].frameId === 1, "Second target frameId should be 1");

  console.log("✓ testDocumentIdsToTargets passed");
}

/**
 * Test: Should generate unique documentIds
 */
function testUniqueDocumentIds() {
  const polyfill = new DocumentIdPolyfill({ debug: true });

  const ids = new Set<string>();
  for (let i = 0; i < 100; i++) {
    const id = polyfill.generateDocumentId(1, 0);
    console.assert(!ids.has(id), "DocumentId should be unique");
    ids.add(id);
  }

  console.log("✓ testUniqueDocumentIds passed");
}

// =============================================================================
// Test Suite: API Wrapping
// =============================================================================

/**
 * Test: Should wrap scripting API correctly
 */
async function testWrapScriptingAPI() {
  const polyfill = new DocumentIdPolyfill({ debug: true });
  const docId = polyfill.generateDocumentId(42, 0);

  let capturedDetails: unknown = null;
  const mockScripting = {
    executeScript: (details: unknown) => {
      capturedDetails = details;
      return Promise.resolve([{ result: "success" }]);
    },
  };

  const wrapped = wrapScriptingAPI(polyfill, mockScripting);

  await wrapped.executeScript({
    target: {
      documentIds: [docId],
    },
    func: () => {},
  });

  console.assert(capturedDetails !== null, "Should have captured details");
  console.assert(
    // deno-lint-ignore no-explicit-any
    (capturedDetails as any).target.tabId === 42,
    "Should convert documentId to tabId",
  );
  console.assert(
    // deno-lint-ignore no-explicit-any
    (capturedDetails as any).target.documentIds === undefined,
    "Should remove documentIds from target",
  );

  console.log("✓ testWrapScriptingAPI passed");
}

/**
 * Test: Should wrap tabs API correctly
 */
async function testWrapTabsAPI() {
  const polyfill = new DocumentIdPolyfill({ debug: true });
  const docId = polyfill.generateDocumentId(55, 3);

  let capturedOptions: unknown = null;
  const mockTabs = {
    sendMessage: (
      _tabId: number,
      _message: unknown,
      options: unknown,
    ) => {
      capturedOptions = options;
      return Promise.resolve({ response: true });
    },
  };

  const wrapped = wrapTabsAPI(polyfill, mockTabs);

  await wrapped.sendMessage(55, { action: "test" }, { documentId: docId });

  console.assert(capturedOptions !== null, "Should have captured options");
  console.assert(
    // deno-lint-ignore no-explicit-any
    (capturedOptions as any).frameId === 3,
    "Should convert documentId to frameId",
  );
  console.assert(
    // deno-lint-ignore no-explicit-any
    (capturedOptions as any).documentId === undefined,
    "Should remove documentId from options",
  );

  console.log("✓ testWrapTabsAPI passed");
}

/**
 * Test: Should pass through when no documentId
 */
async function testPassThroughWithoutDocumentId() {
  const polyfill = new DocumentIdPolyfill({ debug: true });

  let capturedDetails: unknown = null;
  const mockScripting = {
    executeScript: (details: unknown) => {
      capturedDetails = details;
      return Promise.resolve([{ result: "success" }]);
    },
  };

  const wrapped = wrapScriptingAPI(polyfill, mockScripting);

  const originalDetails = {
    target: { tabId: 99, frameIds: [0] },
    func: () => {},
  };

  await wrapped.executeScript(originalDetails);

  console.assert(
    // deno-lint-ignore no-explicit-any
    (capturedDetails as any).target.tabId === 99,
    "Should pass through original tabId",
  );

  console.log("✓ testPassThroughWithoutDocumentId passed");
}

// =============================================================================
// Test Runner
// =============================================================================

/**
 * Run all tests
 */
async function runAllTests() {
  console.log("Running DocumentId API Polyfill Tests...\n");

  const tests = [
    // Generation and Parsing
    { name: "Generate documentId", fn: testGenerateDocumentId },
    { name: "Parse documentId", fn: testParseDocumentId },
    { name: "Parse invalid documentId", fn: testParseInvalidDocumentId },
    { name: "Validate documentId", fn: testIsValidDocumentId },
    { name: "Convert documentIds to targets", fn: testDocumentIdsToTargets },
    { name: "Unique documentIds", fn: testUniqueDocumentIds },

    // API Wrapping
    { name: "Wrap scripting API", fn: testWrapScriptingAPI },
    { name: "Wrap tabs API", fn: testWrapTabsAPI },
    { name: "Pass through without documentId", fn: testPassThroughWithoutDocumentId },
  ];

  let passed = 0;
  let failed = 0;

  for (const test of tests) {
    try {
      console.log(`Running: ${test.name}`);
      await test.fn();
      passed++;
    } catch (error) {
      console.error(`✗ ${test.name} failed:`, error);
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
  testGenerateDocumentId,
  testParseDocumentId,
  testParseInvalidDocumentId,
  testIsValidDocumentId,
  testDocumentIdsToTargets,
  testUniqueDocumentIds,
  testWrapScriptingAPI,
  testWrapTabsAPI,
  testPassThroughWithoutDocumentId,
  runAllTests,
};

// Test file detection constant
const TEST_FILE_NAME = "DocumentIdPolyfill.test";

// Process type for Node.js/Deno compatibility
declare const process:
  | { argv: string[]; exit: (code: number) => never }
  | undefined;

// Run tests if this file is executed directly
if (
  typeof process !== "undefined" &&
  process.argv[1]?.includes(TEST_FILE_NAME)
) {
  runAllTests().then((success) => {
    process!.exit(success ? 0 : 1);
  });
}
