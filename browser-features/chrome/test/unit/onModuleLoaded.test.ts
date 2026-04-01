// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

type TestCase = {
  name: string;
  fn: () => void | Promise<void>;
};

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

function testServicesGlobalAvailable(): void {
  assert(
    typeof Services !== "undefined",
    "Services is not available in chrome test context",
  );
  assert(
    typeof Services.prefs?.getBoolPref === "function",
    "Services.prefs.getBoolPref is not available",
  );
}

function testChromeUtilsGlobalAvailable(): void {
  assert(
    typeof ChromeUtils !== "undefined",
    "ChromeUtils is not available in chrome test context",
  );
  assert(
    typeof ChromeUtils.importESModule === "function",
    "ChromeUtils.importESModule is not available",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "Services global available", fn: testServicesGlobalAvailable },
    {
      name: "ChromeUtils global available",
      fn: testChromeUtilsGlobalAvailable,
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

  if (failures.length > 0) {
    throw new Error(`onModuleLoaded.test.ts failures: ${failures.join(" | ")}`);
  }
}
