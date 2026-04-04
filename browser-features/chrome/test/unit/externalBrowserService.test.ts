// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { externalBrowserService } from "../../common/external-browser/external-browser-service.ts";

type TestCase = {
  name: string;
  fn: () => void | Promise<void>;
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

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

function testServiceIsSingleton(): void {
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

async function testGetInstalledBrowsersReturnsArray(): Promise<void> {
  const browsers = await externalBrowserService.getInstalledBrowsers();
  assert(Array.isArray(browsers), "should return an array");
}

async function testGetBrowserNonExistent(): Promise<void> {
  const browser = await externalBrowserService.getBrowser(
    "nonexistent-browser-id-12345",
  );
  assertEquals(browser, null, "non-existent browser should return null");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "service is singleton", fn: testServiceIsSingleton },
    { name: "has getInstalledBrowsers", fn: testServiceHasGetInstalledBrowsers },
    { name: "has openUrl", fn: testServiceHasOpenUrl },
    { name: "has openInDefaultBrowser", fn: testServiceHasOpenInDefaultBrowser },
    { name: "has getBrowser", fn: testServiceHasGetBrowser },
    { name: "has clearCache", fn: testServiceHasClearCache },
    { name: "getInstalledBrowsers returns array", fn: testGetInstalledBrowsersReturnsArray },
    { name: "getBrowser non-existent", fn: testGetBrowserNonExistent },
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

  if (failures.length > 0) {
    throw new Error(
      `externalBrowserService.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
