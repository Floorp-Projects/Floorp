// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  checkCompatibility,
  extractApiPermissions,
  extractHostPermissions,
  getFirefoxExtensionId,
  transformManifest,
  transformManifestFull,
  validateManifest,
} from "../ManifestTransformer.sys.mts";
import { EXTENSION_ID_SUFFIX, GECKO_MIN_VERSION } from "../Constants.sys.mts";
import type { ChromeManifest } from "../types.ts";

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

function assertDeepEquals(
  actual: unknown,
  expected: unknown,
  message: string,
): void {
  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    throw new Error(
      `${message}\nexpected: ${JSON.stringify(expected)}\nactual:   ${
        JSON.stringify(
          actual,
        )
      }`,
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

const TEST_ID = "abcdefghijklmnopabcdefghijklmnop";

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
  assert(result.browser_specific_settings?.gecko, "should add gecko settings");
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
  const manifest = makeMinimalManifest({
    background: { service_worker: "bg.js", type: "module" },
    permissions: ["offscreen", "scripting"],
    browser_specific_settings: {
      gecko: { id: "original@example.com", strict_min_version: "145.0" },
    },
  });
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
  assertEquals(
    result.background?.type,
    undefined,
    "a default classic worker must not become a module",
  );
  assertEquals(
    result.background?.persistent,
    undefined,
    "MV3 backgrounds must not gain the MV2-only persistent property",
  );
}

function testTransformPreservesCompatibilityIdentity(): void {
  const manifest = makeMinimalManifest({
    browser_specific_settings: {
      gecko: {
        id: "original@example.com",
        strict_min_version: "145.0",
        strict_max_version: "150.*",
      },
    },
  });
  const result = transformManifest(manifest, TEST_ID);
  assertDeepEquals(
    result.browser_specific_settings?.gecko,
    {
      id: `${TEST_ID}${EXTENSION_ID_SUFFIX}`,
      strict_min_version: "145.0",
      strict_max_version: "150.*",
      floorp_chrome_extension: {
        schema_version: 1,
        original_id: TEST_ID,
      },
    },
    "compatibility metadata must preserve the Chrome ID and existing Gecko requirements",
  );
  const override = transformManifestFull(manifest, {
    extensionId: TEST_ID,
    minGeckoVersion: "149.0",
  });
  assertEquals(
    override.manifest.browser_specific_settings?.gecko?.strict_min_version,
    "149.0",
    "an explicit conversion target overrides the manifest minimum",
  );
}

function testTransformPreservesMV3ScriptKind(): void {
  for (const type of ["classic", "module"] as const) {
    const result = transformManifestFull(
      makeMinimalManifest({
        background: { service_worker: "bg.js", type },
        permissions: ["offscreen", "scripting"],
      }),
      { extensionId: TEST_ID },
    );
    assertDeepEquals(
      result.manifest.background,
      { type, scripts: ["bg.js"] },
      `${type} script semantics must survive conversion`,
    );
    assert(
      result.warnings.some(
        (warning) =>
          warning.includes("event page") &&
          warning.includes("service-worker lifecycle"),
      ),
      "conversion must disclose the event-page fallback's limitations",
    );
  }
}

function testTransformPreservesExistingMV3Background(): void {
  const backgrounds: NonNullable<ChromeManifest["background"]>[] = [
    { service_worker: "worker.js", scripts: ["firefox.js"], type: "module" },
    { service_worker: "worker.js", page: "background.html" },
  ];
  for (const background of backgrounds) {
    const manifest = makeMinimalManifest({ background });
    const result = transformManifestFull(manifest, { extensionId: TEST_ID });
    assertEquals(
      result.manifest.background?.service_worker,
      undefined,
      "worker source should not compete with the existing document fallback",
    );
    if ("scripts" in background) {
      assertDeepEquals(
        result.manifest.background?.scripts,
        ["firefox.js"],
        "an existing cross-browser script fallback should be retained",
      );
    } else {
      assertEquals(
        result.manifest.background?.page,
        "background.html",
        "an existing background page should be retained",
      );
      assertEquals(
        result.manifest.background?.scripts,
        undefined,
        "page backgrounds must not gain conflicting scripts",
      );
    }
    assert(
      result.warnings.some((warning) =>
        warning.includes("existing background")
      ),
      "the selected fallback should be reported",
    );
  }
}

function testTransformMV2BackgroundUnchanged(): void {
  const manifest = makeMinimalManifest({
    manifest_version: 2,
    permissions: ["offscreen", "scripting"],
    background: { scripts: ["classic.js"], persistent: false },
  });
  const result = transformManifest(manifest, TEST_ID);
  assertDeepEquals(
    result.background,
    manifest.background,
    "MV2 classic event pages must not become modules when permissions are present",
  );
}

function testTransformRemovesUnsupportedPermissions(): void {
  const manifest = makeMinimalManifest({
    permissions: ["tabs", "processes", "storage"],
  });
  const result = transformManifestFull(manifest, { extensionId: TEST_ID });
  assert(
    !result.manifest.permissions?.includes("processes"),
    "processes should be removed",
  );
  assert(result.manifest.permissions?.includes("tabs"), "tabs should remain");
  assert(
    result.removedPermissions.includes("processes"),
    "processes should be in removedPermissions",
  );
}

function testTransformRemovesUpdateUrl(): void {
  const manifest = makeMinimalManifest();
  (manifest as Record<string, unknown>).update_url =
    "https://example.com/update";
  const result = transformManifest(manifest, TEST_ID);
  assertEquals("update_url" in result, false, "update_url should be removed");
}

function testTransformRemovesKey(): void {
  const manifest = makeMinimalManifest();
  (manifest as Record<string, unknown>).key = "some-key";
  const result = transformManifest(manifest, TEST_ID);
  assertEquals("key" in result, false, "key should be removed");
}

function testTransformNativeOffscreenPreserved(): void {
  const manifest = makeMinimalManifest({
    permissions: ["offscreen", "storage"],
    background: { scripts: ["bg.js"] },
  });
  const result = transformManifest(manifest, TEST_ID);
  assertDeepEquals(
    result.background,
    manifest.background,
    "offscreen must use the runtime host without injecting background-owned scripts",
  );
  assert(
    result.permissions?.includes("offscreen"),
    "the native offscreen permission should remain",
  );
}

function testTransformNativeDocumentIdPreserved(): void {
  const manifest = makeMinimalManifest({
    permissions: ["scripting"],
    background: { scripts: ["bg.js"] },
  });
  const result = transformManifest(manifest, TEST_ID);
  assertDeepEquals(
    result.background,
    manifest.background,
    "native document IDs must not be overridden by synthetic background scripts",
  );
}

function testTransformNativeTabCapturePreserved(): void {
  const manifest = makeMinimalManifest({
    permissions: [
      "tabs",
      "tabCapture",
      "desktopCapture",
      "fontSettings",
      "tts",
      "declarativeNetRequestFeedback",
    ],
    optional_permissions: [
      "tabCapture",
      "desktopCapture",
      "fontSettings",
      "tts",
      "declarativeNetRequestFeedback",
      "alarms",
    ],
  });
  const result = transformManifestFull(manifest, { extensionId: TEST_ID });
  assertDeepEquals(
    result.manifest.permissions,
    [
      "tabs",
      "tabCapture",
      "desktopCapture",
      "fontSettings",
      "tts",
      "declarativeNetRequestFeedback",
    ],
    "permissions with real native backends are retained",
  );
  assertDeepEquals(
    result.manifest.optional_permissions,
    [
      "tabCapture",
      "desktopCapture",
      "fontSettings",
      "tts",
      "declarativeNetRequestFeedback",
      "alarms",
    ],
    "optional native API permissions are retained",
  );
  assert(
    !result.removedPermissions.includes("desktopCapture"),
    "implemented desktop capture is retained",
  );
  assert(
    !result.removedPermissions.includes("tabCapture"),
    "implemented video capture is not a compatibility loss",
  );
  assertEquals(
    result.manifest.background,
    undefined,
    "native capture does not inject a synthetic background",
  );
}

function testNativePermissionsDoNotCreateBackground(): void {
  const manifest = makeMinimalManifest({
    permissions: ["offscreen", "scripting", "tabCapture"],
  });
  const result = transformManifest(manifest, TEST_ID);
  assertEquals(
    result.background,
    undefined,
    "native API permissions must not create a new background context",
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
    permissions: ["processes", "tabs"],
  });
  const issues = checkCompatibility(manifest);
  assert(issues.length > 0, "should report unsupported permissions");
  assert(
    issues.some((i) => i.includes("processes")),
    "should mention processes",
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

function testCompatibilityMV3ServiceWorker(): void {
  const issues = checkCompatibility(
    makeMinimalManifest({
      background: { service_worker: "bg.js", type: "module" },
    }),
  );
  assert(
    issues.some(
      (issue) =>
        issue.includes("event-page fallback") &&
        issue.includes("worker-only APIs"),
    ),
    "MV3 compatibility checks must disclose the worker fallback's limitations",
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
  assertDeepEquals(apis, ["tabs", "storage"], "should extract API permissions");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "validateManifest valid", fn: testValidateManifestValid },
    { name: "validateManifest MV2", fn: testValidateManifestMV2 },
    { name: "validateManifest null", fn: testValidateManifestNull },
    {
      name: "validateManifest missing name",
      fn: testValidateManifestMissingName,
    },
    {
      name: "validateManifest missing version",
      fn: testValidateManifestMissingVersion,
    },
    { name: "validateManifest bad MV", fn: testValidateManifestBadMV },
    {
      name: "transform adds gecko settings",
      fn: testTransformAddsGeckoSettings,
    },
    {
      name: "transform does not mutate original",
      fn: testTransformDoesNotMutateOriginal,
    },
    {
      name: "transform MV3 service_worker",
      fn: testTransformMV3ServiceWorkerConverted,
    },
    {
      name: "transform preserves compatibility identity",
      fn: testTransformPreservesCompatibilityIdentity,
    },
    {
      name: "transform preserves MV3 script kind",
      fn: testTransformPreservesMV3ScriptKind,
    },
    {
      name: "transform preserves existing MV3 document fallback",
      fn: testTransformPreservesExistingMV3Background,
    },
    {
      name: "transform preserves MV2 classic event page",
      fn: testTransformMV2BackgroundUnchanged,
    },
    {
      name: "transform removes unsupported perms",
      fn: testTransformRemovesUnsupportedPermissions,
    },
    { name: "transform removes update_url", fn: testTransformRemovesUpdateUrl },
    { name: "transform removes key", fn: testTransformRemovesKey },
    {
      name: "transform preserves native offscreen",
      fn: testTransformNativeOffscreenPreserved,
    },
    {
      name: "transform preserves native documentId",
      fn: testTransformNativeDocumentIdPreserved,
    },
    {
      name: "transform preserves native video tabCapture",
      fn: testTransformNativeTabCapturePreserved,
    },
    {
      name: "native API permissions do not create backgrounds",
      fn: testNativePermissionsDoNotCreateBackground,
    },
    { name: "compatibility clean", fn: testCompatibilityClean },
    {
      name: "compatibility unsupported perms",
      fn: testCompatibilityUnsupportedPerms,
    },
    {
      name: "compatibility MV2 service worker",
      fn: testCompatibilityMV2ServiceWorker,
    },
    {
      name: "compatibility MV3 service worker",
      fn: testCompatibilityMV3ServiceWorker,
    },
    { name: "getFirefoxExtensionId", fn: testGetFirefoxExtensionId },
    { name: "extractHostPermissions", fn: testExtractHostPermissions },
    { name: "extractApiPermissions", fn: testExtractApiPermissions },
  ];

  await runTests("ManifestTransformer.test.mts", tests);
}
