// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { externalBrowserService } from "../external-browser-service.ts";
import {
  assert,
  assertEquals,
  assertNotEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Basic existence & method checks
// ---------------------------------------------------------------------------

function testServiceIsDefined(): void {
  assert(
    externalBrowserService !== null && externalBrowserService !== undefined,
    "externalBrowserService should be defined",
  );
}

function testServiceHasGetInstalledBrowsers(): void {
  assertEquals(
    typeof externalBrowserService.getInstalledBrowsers,
    "function",
    "should have getInstalledBrowsers method",
  );
}

function testServiceHasOpenUrl(): void {
  assertEquals(
    typeof externalBrowserService.openUrl,
    "function",
    "should have openUrl method",
  );
}

function testServiceHasOpenInDefaultBrowser(): void {
  assertEquals(
    typeof externalBrowserService.openInDefaultBrowser,
    "function",
    "should have openInDefaultBrowser method",
  );
}

function testServiceHasGetBrowser(): void {
  assertEquals(
    typeof externalBrowserService.getBrowser,
    "function",
    "should have getBrowser method",
  );
}

function testServiceHasClearCache(): void {
  assertEquals(
    typeof externalBrowserService.clearCache,
    "function",
    "should have clearCache method",
  );
}

// ---------------------------------------------------------------------------
// Singleton pattern
// ---------------------------------------------------------------------------

async function testSingletonReturnsSameInstance(): Promise<void> {
  // The module-level singleton should be the same across imports.
  // We verify the object identity by checking a property of the reference.
  const { externalBrowserService: secondRef } =
    await import("../external-browser-service.ts");
  assertEquals(
    externalBrowserService,
    secondRef,
    "importing the module again should return the same singleton instance",
  );
}

// ---------------------------------------------------------------------------
// getInstalledBrowsers
// ---------------------------------------------------------------------------

async function testGetInstalledBrowsersReturnsArray(): Promise<void> {
  const browsers = await externalBrowserService.getInstalledBrowsers();
  assert(Array.isArray(browsers), "should return an array");
}

async function testGetInstalledBrowsersReturnsValidShape(): Promise<void> {
  const browsers = await externalBrowserService.getInstalledBrowsers();

  for (const browser of browsers) {
    assertEquals(
      typeof browser.id,
      "string",
      `browser.id should be string, got: ${typeof browser.id}`,
    );
    assert(browser.id.length > 0, `browser.id should be non-empty string`);

    assertEquals(
      typeof browser.name,
      "string",
      `browser.name should be string for "${browser.id}"`,
    );
    assert(
      browser.name.length > 0,
      `browser.name should be non-empty for "${browser.id}"`,
    );

    assertEquals(
      typeof browser.path,
      "string",
      `browser.path should be string for "${browser.id}"`,
    );
    assert(
      browser.path.length > 0,
      `browser.path should be non-empty for "${browser.id}"`,
    );

    assertEquals(
      typeof browser.available,
      "boolean",
      `browser.available should be boolean for "${browser.id}"`,
    );

    // icon is optional
    if (browser.icon !== undefined) {
      assertEquals(
        typeof browser.icon,
        "string",
        `browser.icon should be string when present for "${browser.id}"`,
      );
    }
  }
}

async function testGetInstalledBrowsersWithForceRefresh(): Promise<void> {
  const browsers = await externalBrowserService.getInstalledBrowsers(true);
  assert(
    Array.isArray(browsers),
    "forceRefresh=true should still return an array",
  );
}

async function testGetInstalledBrowsersIdsAreUnique(): Promise<void> {
  const browsers = await externalBrowserService.getInstalledBrowsers();
  const ids = browsers.map((b) => b.id);
  const uniqueIds = new Set(ids);
  assertEquals(ids.length, uniqueIds.size, "browser IDs should be unique");
}

async function testGetInstalledBrowsersCachingBehavior(): Promise<void> {
  // Two consecutive calls without forceRefresh should return consistent data
  const first = await externalBrowserService.getInstalledBrowsers(false);
  const second = await externalBrowserService.getInstalledBrowsers(false);

  assertEquals(
    first.length,
    second.length,
    "cached results should have consistent length",
  );

  for (let i = 0; i < first.length; i++) {
    assertEquals(
      first[i].id,
      second[i].id,
      `cached browser at index ${i} should have same ID`,
    );
  }
}

// ---------------------------------------------------------------------------
// getBrowser
// ---------------------------------------------------------------------------

async function testGetBrowserNonExistent(): Promise<void> {
  const browser = await externalBrowserService.getBrowser(
    "nonexistent-browser-id-12345",
  );
  assertEquals(browser, null, "non-existent browser should return null");
}

async function testGetBrowserReturnsValidBrowserById(): Promise<void> {
  const browsers = await externalBrowserService.getInstalledBrowsers();

  if (browsers.length === 0) {
    // No browsers installed – nothing to validate; pass vacuously
    return;
  }

  const target = browsers[0];
  const result = await externalBrowserService.getBrowser(target.id);

  assert(result !== null, `getBrowser("${target.id}") should not return null`);
  assertEquals(
    result!.id,
    target.id,
    "returned browser ID should match requested ID",
  );
  assertEquals(result!.name, target.name, "returned browser name should match");
  assertEquals(result!.path, target.path, "returned browser path should match");
}

async function testGetBrowserReturnsNullForEmptyId(): Promise<void> {
  const browser = await externalBrowserService.getBrowser("");
  assertEquals(browser, null, "empty string ID should return null");
}

// ---------------------------------------------------------------------------
// openUrl
// ---------------------------------------------------------------------------

async function testOpenUrlReturnsResultShape(): Promise<void> {
  const result = await externalBrowserService.openUrl(
    "https://example.com",
    undefined,
  );

  assert(
    result !== null && result !== undefined,
    "openUrl should return a result object",
  );
  assertEquals(
    typeof result.success,
    "boolean",
    "result.success should be a boolean",
  );

  if (!result.success && result.error !== undefined) {
    assertEquals(
      typeof result.error,
      "string",
      "result.error should be string when present",
    );
  }
}

async function testOpenUrlWithSpecificBrowserId(): Promise<void> {
  const browsers = await externalBrowserService.getInstalledBrowsers();

  if (browsers.length === 0) {
    return; // vacuously pass
  }

  const result = await externalBrowserService.openUrl(
    "https://example.com",
    browsers[0].id,
  );

  assert(
    result !== null && result !== undefined,
    "openUrl with browserId should return a result object",
  );
  assertEquals(
    typeof result.success,
    "boolean",
    "result.success should be boolean",
  );
}

async function testOpenUrlWithPrivateMode(): Promise<void> {
  const result = await externalBrowserService.openUrl(
    "https://example.com",
    undefined,
    true,
  );

  assert(
    result !== null && result !== undefined,
    "openUrl with privateMode should return a result object",
  );
  assertEquals(
    typeof result.success,
    "boolean",
    "result.success should be boolean with privateMode=true",
  );
}

// ---------------------------------------------------------------------------
// openInDefaultBrowser
// ---------------------------------------------------------------------------

async function testOpenInDefaultBrowserReturnsResultShape(): Promise<void> {
  const result = await externalBrowserService.openInDefaultBrowser(
    "https://example.com",
  );

  assert(
    result !== null && result !== undefined,
    "openInDefaultBrowser should return a result object",
  );
  assertEquals(
    typeof result.success,
    "boolean",
    "result.success should be a boolean",
  );

  if (!result.success && result.error !== undefined) {
    assertEquals(
      typeof result.error,
      "string",
      "result.error should be string when present",
    );
  }
}

// ---------------------------------------------------------------------------
// clearCache
// ---------------------------------------------------------------------------

function testClearCacheDoesNotThrow(): void {
  // Should not throw under any circumstance
  externalBrowserService.clearCache();
}

async function testClearCacheInvalidatesBrowserList(): Promise<void> {
  // Fetch browsers, clear cache, fetch again – both should succeed
  const before = await externalBrowserService.getInstalledBrowsers();
  externalBrowserService.clearCache();
  const after = await externalBrowserService.getInstalledBrowsers();

  assert(
    Array.isArray(before),
    "browsers before clearCache should be an array",
  );
  assert(Array.isArray(after), "browsers after clearCache should be an array");
  // After clearing cache the list should be re-fetched but same count is expected
  assertEquals(
    before.length,
    after.length,
    "browser count should remain consistent after cache clear",
  );
}

// ---------------------------------------------------------------------------
// Integration: multiple operations in sequence
// ---------------------------------------------------------------------------

async function testSequentialOperations(): Promise<void> {
  // Exercise multiple service methods in sequence to verify no state corruption
  const browsers = await externalBrowserService.getInstalledBrowsers();
  assert(Array.isArray(browsers), "first call should succeed");

  externalBrowserService.clearCache();

  const browsersAgain = await externalBrowserService.getInstalledBrowsers(true);
  assert(
    Array.isArray(browsersAgain),
    "second call after clearCache should succeed",
  );

  if (browsersAgain.length > 0) {
    const fetched = await externalBrowserService.getBrowser(
      browsersAgain[0].id,
    );
    assert(fetched !== null, "getBrowser should find the browser");
    assertNotEquals(fetched!.name, "", "fetched browser should have a name");
  }

  const result = await externalBrowserService.openInDefaultBrowser(
    "https://example.com",
  );
  assertEquals(
    typeof result.success,
    "boolean",
    "openInDefaultBrowser result should have boolean success",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

const tests: TestCase[] = [
  // Existence & method checks
  { name: "service is defined", fn: testServiceIsDefined },
  { name: "has getInstalledBrowsers", fn: testServiceHasGetInstalledBrowsers },
  { name: "has openUrl", fn: testServiceHasOpenUrl },
  { name: "has openInDefaultBrowser", fn: testServiceHasOpenInDefaultBrowser },
  { name: "has getBrowser", fn: testServiceHasGetBrowser },
  { name: "has clearCache", fn: testServiceHasClearCache },

  // Singleton
  {
    name: "singleton returns same instance",
    fn: testSingletonReturnsSameInstance,
  },

  // getInstalledBrowsers
  {
    name: "getInstalledBrowsers returns array",
    fn: testGetInstalledBrowsersReturnsArray,
  },
  {
    name: "getInstalledBrowsers returns valid shape",
    fn: testGetInstalledBrowsersReturnsValidShape,
  },
  {
    name: "getInstalledBrowsers with forceRefresh",
    fn: testGetInstalledBrowsersWithForceRefresh,
  },
  {
    name: "getInstalledBrowsers IDs are unique",
    fn: testGetInstalledBrowsersIdsAreUnique,
  },
  {
    name: "getInstalledBrowsers caching behavior",
    fn: testGetInstalledBrowsersCachingBehavior,
  },

  // getBrowser
  { name: "getBrowser non-existent", fn: testGetBrowserNonExistent },
  {
    name: "getBrowser returns valid browser by ID",
    fn: testGetBrowserReturnsValidBrowserById,
  },
  {
    name: "getBrowser returns null for empty ID",
    fn: testGetBrowserReturnsNullForEmptyId,
  },

  // openUrl
  {
    name: "openUrl returns result shape",
    fn: testOpenUrlReturnsResultShape,
  },
  {
    name: "openUrl with specific browser ID",
    fn: testOpenUrlWithSpecificBrowserId,
  },
  {
    name: "openUrl with private mode",
    fn: testOpenUrlWithPrivateMode,
  },

  // openInDefaultBrowser
  {
    name: "openInDefaultBrowser returns result shape",
    fn: testOpenInDefaultBrowserReturnsResultShape,
  },

  // clearCache
  {
    name: "clearCache does not throw",
    fn: testClearCacheDoesNotThrow,
  },
  {
    name: "clearCache invalidates browser list",
    fn: testClearCacheInvalidatesBrowserList,
  },

  // Integration
  {
    name: "sequential operations",
    fn: testSequentialOperations,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("externalBrowserService.test.ts", tests);
}
