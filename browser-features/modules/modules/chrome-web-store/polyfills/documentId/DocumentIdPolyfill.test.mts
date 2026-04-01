/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @colocated-env browser

/**
 * Unit tests for DocumentId API Polyfill
 *
 * Browser-only, deterministic tests that avoid Node-specific helpers.
 */

import {
  DocumentIdPolyfill,
  wrapScriptingAPI,
  wrapTabsAPI,
} from "./DocumentIdPolyfill.sys.mts";
import {
  assert,
  assertEquals,
  assertRejects,
} from "../test-helpers/assertions.mts";

function createPolyfill(): DocumentIdPolyfill {
  return new DocumentIdPolyfill({ debug: true });
}

// =============================================================================
// Test Suite: DocumentId Generation and Parsing
// =============================================================================

/**
 * Test: Should generate and parse documentId consistently
 */
function testGenerateAndParseRoundTrip() {
  const polyfill = createPolyfill();

  const documentId = polyfill.generateDocumentId(123, 7);

  assert(
    documentId.startsWith("floorp-doc-"),
    "DocumentId should start with floorp-doc-",
  );
  const parsed = polyfill.parseDocumentId(documentId);

  assert(parsed !== null, "Parsed result should not be null");
  assertEquals(parsed.tabId, 123, "tabId should round-trip correctly");
  assertEquals(parsed.frameId, 7, "frameId should round-trip correctly");

  console.log("✓ testGenerateAndParseRoundTrip passed");
}

/**
 * Test: Should return null for invalid documentId
 */
function testParseInvalidDocumentId() {
  const polyfill = createPolyfill();

  const parsed1 = polyfill.parseDocumentId("invalid-id");
  assertEquals(parsed1, null, "Should return null for invalid prefix");

  const parsed2 = polyfill.parseDocumentId("floorp-doc-abc-def");
  assertEquals(parsed2, null, "Should return null for non-numeric values");

  console.log("✓ testParseInvalidDocumentId passed");
}

/**
 * Test: Should validate documentId correctly
 */
function testIsValidDocumentId() {
  const polyfill = createPolyfill();

  const documentId = polyfill.generateDocumentId(789, 1);

  assertEquals(
    polyfill.isValidDocumentId(documentId),
    true,
    "Generated documentId should be valid",
  );
  assertEquals(
    polyfill.isValidDocumentId("invalid"),
    false,
    "Invalid documentId should not be valid",
  );

  console.log("✓ testIsValidDocumentId passed");
}

/**
 * Test: Should convert documentIds to targets and skip invalid values
 */
function testDocumentIdsToTargets() {
  const polyfill = createPolyfill();

  const docId1 = polyfill.generateDocumentId(100, 0);
  const docId2 = polyfill.generateDocumentId(200, 1);

  const targets = polyfill.documentIdsToTargets([docId1, docId2, "invalid"]);

  assertEquals(targets.length, 2, "Should have 2 valid targets");
  assertEquals(targets[0].tabId, 100, "First target tabId should be 100");
  assertEquals(targets[0].frameId, 0, "First target frameId should be 0");
  assertEquals(targets[1].tabId, 200, "Second target tabId should be 200");
  assertEquals(targets[1].frameId, 1, "Second target frameId should be 1");

  console.log("✓ testDocumentIdsToTargets passed");
}

/**
 * Test: Should generate unique documentIds
 */
function testUniqueDocumentIds() {
  const polyfill = createPolyfill();

  const ids = new Set<string>();
  for (let i = 0; i < 100; i++) {
    const id = polyfill.generateDocumentId(1, 0);
    assert(!ids.has(id), "DocumentId should be unique");
    ids.add(id);
  }

  console.log("✓ testUniqueDocumentIds passed");
}

/**
 * Test: Should prune cache when exceeding limit
 */
function testCachePruning() {
  const polyfill = createPolyfill();

  // Generate many IDs to exceed MAX_CACHE_SIZE and trigger pruning.
  let latestId = "";
  for (let i = 0; i < 1100; i++) {
    latestId = polyfill.generateDocumentId(i, i % 3);
  }

  const parsed = polyfill.parseDocumentId(latestId);

  assert(parsed !== null, "Recent documentId should still be parseable");
  assertEquals(parsed.tabId, 1099, "tabId should match latest entry");
  assertEquals(parsed.frameId, 1, "frameId should match latest entry");

  console.log("✓ testCachePruning passed");
}

// =============================================================================
// Test Suite: API Wrapping
// =============================================================================

/**
 * Test: Should wrap scripting API correctly
 */
async function testWrapScriptingAPI() {
  const polyfill = createPolyfill();
  const docId1 = polyfill.generateDocumentId(42, 0);
  const docId2 = polyfill.generateDocumentId(42, 3);

  const capturedDetails: Array<Record<string, unknown>> = [];
  const mockScripting = {
    executeScript: (details: Record<string, unknown>) => {
      capturedDetails.push(details);
      return Promise.resolve([{ callIndex: capturedDetails.length }]);
    },
  };

  const wrapped = wrapScriptingAPI(polyfill, mockScripting);

  const result = await wrapped.executeScript({
    target: {
      documentIds: [docId1, docId2],
    },
    func: () => {},
  });

  assertEquals(capturedDetails.length, 2, "Should execute once per documentId");
  assert(Array.isArray(result), "Wrapped result should be an array");
  assertEquals(
    result.length,
    2,
    "Wrapped result should merge execution results",
  );

  const firstTarget = capturedDetails[0].target as Record<string, unknown>;
  const secondTarget = capturedDetails[1].target as Record<string, unknown>;

  assert(firstTarget.tabId === 42, "First execution should keep tabId");
  assert(
    Array.isArray(firstTarget.frameIds) &&
      (firstTarget.frameIds as unknown[])[0] === 0,
    "First execution should map frameId from documentId",
  );
  assert(
    Array.isArray(secondTarget.frameIds) &&
      (secondTarget.frameIds as unknown[])[0] === 3,
    "Second execution should map frameId from documentId",
  );
  assert(
    firstTarget.documentIds === undefined &&
      secondTarget.documentIds === undefined,
    "Wrapped executions should remove documentIds from target",
  );

  console.log("✓ testWrapScriptingAPI passed");
}

/**
 * Test: Should reject when all documentIds are invalid
 */
async function testWrapScriptingRejectsInvalidDocumentIds() {
  const polyfill = createPolyfill();
  const mockScripting = {
    executeScript: () => Promise.resolve([] as unknown[]),
  };
  const wrapped = wrapScriptingAPI(polyfill, mockScripting);

  await assertRejects(
    () =>
      wrapped.executeScript({
        target: { documentIds: ["invalid-document-id"] },
        func: () => {},
      }),
    "No valid documentIds provided",
    "Invalid documentIds should cause rejection",
  );

  console.log("✓ testWrapScriptingRejectsInvalidDocumentIds passed");
}

/**
 * Test: Should wrap tabs API correctly
 */
async function testWrapTabsAPI() {
  const polyfill = createPolyfill();
  const docId = polyfill.generateDocumentId(55, 3);

  let capturedOptions: unknown = null;
  const mockTabs = {
    sendMessage: (_tabId: number, _message: unknown, options: unknown) => {
      capturedOptions = options;
      return Promise.resolve({ response: true });
    },
  };

  const wrapped = wrapTabsAPI(polyfill, mockTabs);

  await wrapped.sendMessage(55, { action: "test" }, { documentId: docId });

  assert(capturedOptions !== null, "Should have captured options");
  assert(
    // deno-lint-ignore no-explicit-any
    (capturedOptions as any).frameId === 3,
    "Should convert documentId to frameId",
  );
  assert(
    // deno-lint-ignore no-explicit-any
    (capturedOptions as any).documentId === undefined,
    "Should remove documentId from options",
  );

  console.log("✓ testWrapTabsAPI passed");
}

/**
 * Test: Should reject tabs.sendMessage when documentId is invalid
 */
async function testWrapTabsRejectsInvalidDocumentId() {
  const polyfill = createPolyfill();
  const mockTabs = {
    sendMessage: () => Promise.resolve({ ok: true }),
  };
  const wrapped = wrapTabsAPI(polyfill, mockTabs);

  await assertRejects(
    () => wrapped.sendMessage(1, { ping: true }, { documentId: "invalid" }),
    "Invalid documentId",
    "Invalid documentId should reject wrapped tabs API",
  );

  console.log("✓ testWrapTabsRejectsInvalidDocumentId passed");
}

/**
 * Test: Should pass through when no documentId
 */
async function testPassThroughWithoutDocumentId() {
  const polyfill = createPolyfill();

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

  assert(
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

  const tests: Array<{ name: string; fn: () => void | Promise<void> }> = [
    // Generation and Parsing
    {
      name: "Generate and parse documentId",
      fn: testGenerateAndParseRoundTrip,
    },
    { name: "Parse invalid documentId", fn: testParseInvalidDocumentId },
    { name: "Validate documentId", fn: testIsValidDocumentId },
    { name: "Convert documentIds to targets", fn: testDocumentIdsToTargets },
    { name: "Unique documentIds", fn: testUniqueDocumentIds },
    { name: "Cache pruning", fn: testCachePruning },

    // API Wrapping
    { name: "Wrap scripting API", fn: testWrapScriptingAPI },
    {
      name: "Wrap scripting API invalid input",
      fn: testWrapScriptingRejectsInvalidDocumentIds,
    },
    { name: "Wrap tabs API", fn: testWrapTabsAPI },
    {
      name: "Wrap tabs API invalid input",
      fn: testWrapTabsRejectsInvalidDocumentId,
    },
    {
      name: "Pass through without documentId",
      fn: testPassThroughWithoutDocumentId,
    },
  ];

  let passed = 0;
  let failed = 0;
  const failureDetails: string[] = [];

  for (const test of tests) {
    try {
      console.log(`Running: ${test.name}`);
      await test.fn();
      passed++;
    } catch (error) {
      console.error(`✗ ${test.name} failed:`, error);
      failed++;
      const message = error instanceof Error ? error.message : String(error);
      failureDetails.push(`${test.name}: ${message}`);
    }
  }

  console.log(`\nTest Results: ${passed} passed, ${failed} failed`);

  if (failed > 0) {
    throw new Error(`DocumentId test failures: ${failureDetails.join(" | ")}`);
  }

  return true;
}

// =============================================================================
// Export for test framework integration
// =============================================================================

export {
  testGenerateAndParseRoundTrip,
  testParseInvalidDocumentId,
  testIsValidDocumentId,
  testDocumentIdsToTargets,
  testUniqueDocumentIds,
  testCachePruning,
  testWrapScriptingAPI,
  testWrapScriptingRejectsInvalidDocumentIds,
  testWrapTabsAPI,
  testWrapTabsRejectsInvalidDocumentId,
  testPassThroughWithoutDocumentId,
  runAllTests,
};
