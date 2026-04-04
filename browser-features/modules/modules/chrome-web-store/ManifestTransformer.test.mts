// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  transformManifest,
  transformManifestFull,
  validateManifest,
  checkCompatibility,
  getFirefoxExtensionId,
  extractHostPermissions,
  extractApiPermissions,
} from "./ManifestTransformer.sys.mts";
import { EXTENSION_ID_SUFFIX, GECKO_MIN_VERSION } from "./Constants.sys.mts";
import type { ChromeManifest } from "./types.ts";

type TestCase = {
  name: string;
  fn: () => void;
};

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected) {
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
  }
}

function assertDeepEquals(
  actual: unknown,
  expected: unknown,
  message: string,
): void {
  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    throw new Error(
      `${message}\nexpected: ${JSON.stringify(expected)}\nactual:   ${JSON.stringify(actual)}`,
    );
  }
}

function makeMinimalManifest(
  overrides: Partial<ChromeManifest> = {},
): ChromeManifest {
  return {
    manifest_version: 3,
    name: "Test Extension",
    version: "1.0",
    ...overrides,
  } as ChromeManifest;
}

const TEST_ID = "abcdefghijklmnopqrstuvwxyzabcdef";

// ---------------------------------------------------------------------------
// Tests — validateManifest
// ---------------------------------------------------------------------------

function testValidateManifestValid(): void {
  assert(
    validateManifest({ manifest_version: 3, name: "Ext", version: "1.0" }),
    "valid manifest should pass",
  );
}

function testValidateManifestMV2(): void {
  assert(
    validateManifest({ manifest_version: 2, name: "Ext", version: "1.0" }),
    "MV2 manifest should pass",
  );
}

function testValidateManifestNull(): void {
  assertEquals(validateManifest(null), false, "null should fail");
}

function testValidateManifestMissingName(): void {
  assertEquals(
    validateManifest({ manifest_version: 3, name: "", version: "1.0" }),
    false,
    "empty name should fail",
  );
}

function testValidateManifestMissingVersion(): void {
  assertEquals(
    validateManifest({ manifest_version: 3, name: "Ext", version: "" }),
    false,
    "empty version should fail",
  );
}

function testValidateManifestBadMV(): void {
  assertEquals(
    validateManifest({ manifest_version: 1, name: "Ext", version: "1.0" }),
    false,
    "manifest_version 1 should fail",
  );
}

// ---------------------------------------------------------------------------
// Tests — transformManifest
// ---------------------------------------------------------------------------

function testTransformAddsGeckoSettings(): void {
  const manifest = makeMinimalManifest();
  const result = transformManifest(manifest, TEST_ID);
  assert(
    result.browser_specific_settings?.gecko,
    "should add gecko settings",
  );
  assertEquals(
    result.browser_specific_settings!.gecko!.id,
    `${TEST_ID}${EXTENSION_ID_SUFFIX}`,
    "gecko id should match",
  );
  assertEquals(
    result.browser_specific_settings!.gecko!.strict_min_version,
    GECKO_MIN_VERSION,
    "min version should be set",
  );
}

function testTransformDoesNotMutateOriginal(): void {
  const manifest = makeMinimalManifest();
  const original = JSON.stringify(manifest);
  transformManifest(manifest, TEST_ID);
  assertEquals(
    JSON.stringify(manifest),
    original,
    "original manifest should not be mutated",
  );
}

function testTransformMV3ServiceWorkerConverted(): void {
  const manifest = makeMinimalManifest({
    background: { service_worker: "bg.js" },
  });
  const result = transformManifest(manifest, TEST_ID);
  assertEquals(
    result.background?.service_worker,
    undefined,
    "service_worker should be removed",
  );
  assert(
    result.background?.scripts?.includes("bg.js"),
    "bg.js should be in scripts",
  );
}

function testTransformRemovesUnsupportedPermissions(): void {
  const manifest = makeMinimalManifest({
    permissions: ["tabs", "tabCapture", "storage"],
  });
  const result = transformManifestFull(manifest, { extensionId: TEST_ID });
  assert(
    !result.manifest.permissions?.includes("tabCapture"),
    "tabCapture should be removed",
  );
  assert(
    result.manifest.permissions?.includes("tabs"),
    "tabs should remain",
  );
  assert(
    result.removedPermissions.includes("tabCapture"),
    "tabCapture should be in removedPermissions",
  );
}

function testTransformRemovesUpdateUrl(): void {
  const manifest = makeMinimalManifest();
  (manifest as Record<string, unknown>).update_url =
    "https://example.com/update";
  const result = transformManifest(manifest, TEST_ID);
  assertEquals(
    "update_url" in result,
    false,
    "update_url should be removed",
  );
}

function testTransformRemovesKey(): void {
  const manifest = makeMinimalManifest();
  (manifest as Record<string, unknown>).key = "some-key";
  const result = transformManifest(manifest, TEST_ID);
  assertEquals("key" in result, false, "key should be removed");
}

function testTransformOffscreenPolyfillInjected(): void {
  const manifest = makeMinimalManifest({
    permissions: ["offscreen", "storage"],
    background: { scripts: ["bg.js"] },
  });
  const result = transformManifest(manifest, TEST_ID);
  assert(
    result.background?.scripts?.some((s: string) =>
      s.includes("OffscreenPolyfill"),
    ),
    "Offscreen polyfill should be injected",
  );
}

function testTransformScriptingDocumentIdPolyfill(): void {
  const manifest = makeMinimalManifest({
    permissions: ["scripting"],
    background: { scripts: ["bg.js"] },
  });
  const result = transformManifest(manifest, TEST_ID);
  assert(
    result.background?.scripts?.some((s: string) =>
      s.includes("DocumentIdPolyfill"),
    ),
    "DocumentId polyfill should be injected",
  );
}

// ---------------------------------------------------------------------------
// Tests — checkCompatibility
// ---------------------------------------------------------------------------

function testCompatibilityClean(): void {
  const manifest = makeMinimalManifest({ permissions: ["tabs", "storage"] });
  const issues = checkCompatibility(manifest);
  assertEquals(issues.length, 0, "clean manifest should have no issues");
}

function testCompatibilityUnsupportedPerms(): void {
  const manifest = makeMinimalManifest({
    permissions: ["tabCapture", "tabs"],
  });
  const issues = checkCompatibility(manifest);
  assert(issues.length > 0, "should report unsupported permissions");
  assert(
    issues.some((i) => i.includes("tabCapture")),
    "should mention tabCapture",
  );
}

function testCompatibilityMV2ServiceWorker(): void {
  const manifest = makeMinimalManifest({
    manifest_version: 2,
    background: { service_worker: "bg.js" },
  } as Partial<ChromeManifest>);
  const issues = checkCompatibility(manifest);
  assert(
    issues.some((i) => i.includes("service worker")),
    "should warn about service worker in MV2",
  );
}

// ---------------------------------------------------------------------------
// Tests — utility functions
// ---------------------------------------------------------------------------

function testGetFirefoxExtensionId(): void {
  assertEquals(
    getFirefoxExtensionId(TEST_ID),
    `${TEST_ID}${EXTENSION_ID_SUFFIX}`,
    "should append suffix",
  );
}

function testExtractHostPermissions(): void {
  const perms = ["tabs", "https://example.com/*", "<all_urls>", "storage"];
  const hosts = extractHostPermissions(perms);
  assertDeepEquals(
    hosts,
    ["https://example.com/*", "<all_urls>"],
    "should extract host permissions",
  );
}

function testExtractApiPermissions(): void {
  const perms = ["tabs", "https://example.com/*", "<all_urls>", "storage"];
  const apis = extractApiPermissions(perms);
  assertDeepEquals(
    apis,
    ["tabs", "storage"],
    "should extract API permissions",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "validateManifest valid", fn: testValidateManifestValid },
    { name: "validateManifest MV2", fn: testValidateManifestMV2 },
    { name: "validateManifest null", fn: testValidateManifestNull },
    { name: "validateManifest missing name", fn: testValidateManifestMissingName },
    { name: "validateManifest missing version", fn: testValidateManifestMissingVersion },
    { name: "validateManifest bad MV", fn: testValidateManifestBadMV },
    { name: "transform adds gecko settings", fn: testTransformAddsGeckoSettings },
    { name: "transform does not mutate original", fn: testTransformDoesNotMutateOriginal },
    { name: "transform MV3 service_worker", fn: testTransformMV3ServiceWorkerConverted },
    { name: "transform removes unsupported perms", fn: testTransformRemovesUnsupportedPermissions },
    { name: "transform removes update_url", fn: testTransformRemovesUpdateUrl },
    { name: "transform removes key", fn: testTransformRemovesKey },
    { name: "transform offscreen polyfill", fn: testTransformOffscreenPolyfillInjected },
    { name: "transform documentId polyfill", fn: testTransformScriptingDocumentIdPolyfill },
    { name: "compatibility clean", fn: testCompatibilityClean },
    { name: "compatibility unsupported perms", fn: testCompatibilityUnsupportedPerms },
    { name: "compatibility MV2 service worker", fn: testCompatibilityMV2ServiceWorker },
    { name: "getFirefoxExtensionId", fn: testGetFirefoxExtensionId },
    { name: "extractHostPermissions", fn: testExtractHostPermissions },
    { name: "extractApiPermissions", fn: testExtractApiPermissions },
  ];

  const failures: string[] = [];

  for (const test of tests) {
    try {
      test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }

  if (failures.length > 0) {
    throw new Error(
      `ManifestTransformer.test.mts failures: ${failures.join(" | ")}`,
    );
  }
}
