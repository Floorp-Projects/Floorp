// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { externalBrowserService } from "../external-browser-service.ts";
import type {
  ExternalBrowser,
  ExternalBrowserServiceInterface,
} from "../types.ts";
import {
  assert,
  assertEquals,
  assertNotEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

type MutableExternalBrowserServiceWrapper = {
  _service: ExternalBrowserServiceInterface | null;
};

type MockExternalServiceCalls = {
  getInstalledBrowsers: boolean[];
  openUrl: Array<{
    url: string;
    browserId: string | undefined;
    privateMode: boolean;
  }>;
  openInDefaultBrowser: string[];
  getBrowser: string[];
  clearCache: number;
};

const MOCK_BROWSERS: ExternalBrowser[] = [
  {
    id: "mock-browser",
    name: "Mock Browser",
    path: "/mock/browser",
    available: true,
  },
];

function createMockExternalService(): {
  service: ExternalBrowserServiceInterface;
  calls: MockExternalServiceCalls;
} {
  const calls: MockExternalServiceCalls = {
    getInstalledBrowsers: [],
    openUrl: [],
    openInDefaultBrowser: [],
    getBrowser: [],
    clearCache: 0,
  };

  const service: ExternalBrowserServiceInterface = {
    getInstalledBrowsers(forceRefresh = false) {
      calls.getInstalledBrowsers.push(forceRefresh);
      return MOCK_BROWSERS;
    },
    openUrl(url, browserId, privateMode = false) {
      calls.openUrl.push({ url, browserId, privateMode });
      return { success: true };
    },
    openInDefaultBrowser(url) {
      calls.openInDefaultBrowser.push(url);
      return { success: true };
    },
    getBrowser(id) {
      calls.getBrowser.push(id);
      return MOCK_BROWSERS.find((browser) => browser.id === id) ?? null;
    },
    clearCache() {
      calls.clearCache += 1;
    },
  };

  return { service, calls };
}

async function withMockExternalService(
  service: ExternalBrowserServiceInterface,
  fn: () => void | Promise<void>,
): Promise<void> {
  const wrapper =
    externalBrowserService as unknown as MutableExternalBrowserServiceWrapper;
  const previousService = wrapper._service;
  wrapper._service = service;

  try {
    await fn();
  } finally {
    wrapper._service = previousService;
  }
}

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
// Lazy loading and service initialization
// ---------------------------------------------------------------------------

async function testServiceIsLazyLoaded(): Promise<void> {
  // The service should be loaded on first call, not at import time
  // We verify this by checking that calling a method works (which triggers lazy load)
  const browsers = await externalBrowserService.getInstalledBrowsers();
  assert(Array.isArray(browsers), "lazy loading should work on first call");
}

async function testServiceCachesInstance(): Promise<void> {
  // Multiple calls to getService() should return the same instance
  // This is tested implicitly by the singleton test, but we can verify behavior
  const first = await externalBrowserService.getInstalledBrowsers();
  const second = await externalBrowserService.getInstalledBrowsers();

  assertEquals(
    first.length,
    second.length,
    "cached service instance should return consistent results",
  );
}

// ---------------------------------------------------------------------------
// openUrl edge cases
// ---------------------------------------------------------------------------

async function testOpenUrlWithInvalidUrl(): Promise<void> {
  const result = await externalBrowserService.openUrl("not-a-valid-url");

  assert(
    result !== null && result !== undefined,
    "openUrl should return a result object even for invalid URL",
  );
  assertEquals(
    typeof result.success,
    "boolean",
    "result.success should be boolean for invalid URL",
  );
}

async function testOpenUrlWithEmptyString(): Promise<void> {
  const result = await externalBrowserService.openUrl("");

  assert(
    result !== null && result !== undefined,
    "openUrl should return a result object for empty string",
  );
  assertEquals(
    typeof result.success,
    "boolean",
    "result.success should be boolean for empty string",
  );
}

async function testOpenUrlWithNonExistentBrowserId(): Promise<void> {
  const result = await externalBrowserService.openUrl(
    "https://example.com",
    "non-existent-browser-id",
  );

  assert(
    result !== null && result !== undefined,
    "openUrl should return a result object for non-existent browser",
  );
  // Should likely fail but not throw
  assertEquals(
    typeof result.success,
    "boolean",
    "result.success should be boolean",
  );
}

// ---------------------------------------------------------------------------
// getBrowser edge cases
// ---------------------------------------------------------------------------

async function testGetBrowserWithSpecialCharacters(): Promise<void> {
  const result = await externalBrowserService.getBrowser(
    "browser/../with/../paths",
  );

  assertEquals(
    result,
    null,
    "browser ID with path traversal should return null",
  );
}

async function testGetBrowserWithVeryLongId(): Promise<void> {
  const longId = "a".repeat(10000);
  const result = await externalBrowserService.getBrowser(longId);

  assertEquals(result, null, "very long browser ID should return null");
}

// ---------------------------------------------------------------------------
// clearCache edge cases
// ---------------------------------------------------------------------------

function testClearCacheMultipleCalls(): void {
  // Calling clearCache multiple times should not throw
  externalBrowserService.clearCache();
  externalBrowserService.clearCache();
  externalBrowserService.clearCache();
}

async function testClearCacheThenGetBrowser(): Promise<void> {
  // Clear cache and try to get a browser - should still work
  externalBrowserService.clearCache();

  const browsers = await externalBrowserService.getInstalledBrowsers();
  if (browsers.length > 0) {
    const result = await externalBrowserService.getBrowser(browsers[0].id);
    assert(result !== null, "getBrowser should work after clearCache");
  }
}

// ---------------------------------------------------------------------------
// openInDefaultBrowser edge cases
// ---------------------------------------------------------------------------

async function testOpenInDefaultBrowserWithInvalidUrl(): Promise<void> {
  const result = await externalBrowserService.openInDefaultBrowser("not-a-url");

  assert(
    result !== null && result !== undefined,
    "openInDefaultBrowser should return result for invalid URL",
  );
  assertEquals(
    typeof result.success,
    "boolean",
    "result.success should be boolean",
  );
}

async function testOpenInDefaultBrowserWithEmptyString(): Promise<void> {
  const result = await externalBrowserService.openInDefaultBrowser("");

  assert(
    result !== null && result !== undefined,
    "openInDefaultBrowser should return result for empty string",
  );
  assertEquals(
    typeof result.success,
    "boolean",
    "result.success should be boolean",
  );
}

// ---------------------------------------------------------------------------
// Error handling and resilience
// ---------------------------------------------------------------------------

async function testConcurrentGetInstalledBrowsersCalls(): Promise<void> {
  // Multiple concurrent calls should all succeed
  const promises = [
    externalBrowserService.getInstalledBrowsers(),
    externalBrowserService.getInstalledBrowsers(),
    externalBrowserService.getInstalledBrowsers(),
  ];

  const results = await Promise.all(promises);

  for (const result of results) {
    assert(Array.isArray(result), "concurrent calls should return arrays");
    assertEquals(
      results[0].length,
      result.length,
      "concurrent calls should return consistent lengths",
    );
  }
}

async function testGetInstalledBrowsersAfterClear(): Promise<void> {
  const before = await externalBrowserService.getInstalledBrowsers();
  externalBrowserService.clearCache();
  const after = await externalBrowserService.getInstalledBrowsers();

  assert(
    Array.isArray(before) && Array.isArray(after),
    "both calls should return arrays",
  );
  assertEquals(
    before.length,
    after.length,
    "browser count should be consistent",
  );
}

// ---------------------------------------------------------------------------
// Wrapped service delegation behavior (deterministic)
// ---------------------------------------------------------------------------

async function testGetInstalledBrowsersDelegatesForceRefresh(): Promise<void> {
  const { service, calls } = createMockExternalService();

  await withMockExternalService(service, async () => {
    await externalBrowserService.getInstalledBrowsers(true);
    await externalBrowserService.getInstalledBrowsers();
  });

  assertEquals(calls.getInstalledBrowsers.length, 2, "should call twice");
  assertEquals(
    calls.getInstalledBrowsers[0],
    true,
    "first call should forward forceRefresh=true",
  );
  assertEquals(
    calls.getInstalledBrowsers[1],
    false,
    "second call should use wrapper default forceRefresh=false",
  );
}

async function testOpenUrlDelegatesAllArguments(): Promise<void> {
  const { service, calls } = createMockExternalService();

  await withMockExternalService(service, async () => {
    await externalBrowserService.openUrl(
      "https://example.com",
      "mock-browser",
      true,
    );
  });

  assertEquals(calls.openUrl.length, 1, "openUrl should be called once");
  assertEquals(calls.openUrl[0].url, "https://example.com", "url should match");
  assertEquals(
    calls.openUrl[0].browserId,
    "mock-browser",
    "browserId should be forwarded",
  );
  assertEquals(
    calls.openUrl[0].privateMode,
    true,
    "privateMode should be forwarded",
  );
}

async function testGetBrowserDelegatesId(): Promise<void> {
  const { service, calls } = createMockExternalService();

  await withMockExternalService(service, async () => {
    const browser = await externalBrowserService.getBrowser("mock-browser");
    assert(browser !== null, "mock browser should be returned");
  });

  assertEquals(calls.getBrowser.length, 1, "getBrowser should be called once");
  assertEquals(calls.getBrowser[0], "mock-browser", "id should be forwarded");
}

async function testOpenInDefaultBrowserDelegatesUrl(): Promise<void> {
  const { service, calls } = createMockExternalService();

  await withMockExternalService(service, async () => {
    await externalBrowserService.openInDefaultBrowser("https://floorp.app");
  });

  assertEquals(
    calls.openInDefaultBrowser.length,
    1,
    "openInDefaultBrowser should be called once",
  );
  assertEquals(
    calls.openInDefaultBrowser[0],
    "https://floorp.app",
    "url should be forwarded",
  );
}

async function testClearCacheDelegatesToWrappedService(): Promise<void> {
  const { service, calls } = createMockExternalService();

  await withMockExternalService(service, () => {
    externalBrowserService.clearCache();
  });

  assertEquals(calls.clearCache, 1, "clearCache should be delegated once");
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

  // Lazy loading
  {
    name: "service is lazy loaded",
    fn: testServiceIsLazyLoaded,
  },
  {
    name: "service caches instance",
    fn: testServiceCachesInstance,
  },

  // openUrl edge cases
  {
    name: "openUrl with invalid URL",
    fn: testOpenUrlWithInvalidUrl,
  },
  {
    name: "openUrl with empty string",
    fn: testOpenUrlWithEmptyString,
  },
  {
    name: "openUrl with non-existent browser ID",
    fn: testOpenUrlWithNonExistentBrowserId,
  },

  // getBrowser edge cases
  {
    name: "getBrowser with special characters",
    fn: testGetBrowserWithSpecialCharacters,
  },
  {
    name: "getBrowser with very long ID",
    fn: testGetBrowserWithVeryLongId,
  },

  // clearCache edge cases
  {
    name: "clearCache multiple calls",
    fn: testClearCacheMultipleCalls,
  },
  {
    name: "clearCache then getBrowser",
    fn: testClearCacheThenGetBrowser,
  },

  // openInDefaultBrowser edge cases
  {
    name: "openInDefaultBrowser with invalid URL",
    fn: testOpenInDefaultBrowserWithInvalidUrl,
  },
  {
    name: "openInDefaultBrowser with empty string",
    fn: testOpenInDefaultBrowserWithEmptyString,
  },

  // Error handling
  {
    name: "concurrent getInstalledBrowsers calls",
    fn: testConcurrentGetInstalledBrowsersCalls,
  },
  {
    name: "getInstalledBrowsers after clear",
    fn: testGetInstalledBrowsersAfterClear,
  },

  // Wrapped service delegation
  {
    name: "delegates getInstalledBrowsers forceRefresh",
    fn: testGetInstalledBrowsersDelegatesForceRefresh,
  },
  {
    name: "delegates openUrl arguments",
    fn: testOpenUrlDelegatesAllArguments,
  },
  {
    name: "delegates getBrowser id",
    fn: testGetBrowserDelegatesId,
  },
  {
    name: "delegates openInDefaultBrowser url",
    fn: testOpenInDefaultBrowserDelegatesUrl,
  },
  {
    name: "delegates clearCache",
    fn: testClearCacheDelegatesToWrappedService,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("externalBrowserService.test.ts", tests);
}
