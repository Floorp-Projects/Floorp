// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  isTrustedPluginStoreSource,
  isValidPluginId,
  isValidPluginMetadata,
} from "../Policy.sys.mts";

function assertEquals<T>(actual: T, expected: T): void {
  harnessAssertEquals(actual, expected, "values should be equal");
}

function testProductionOrigins(): void {
  assertEquals(
    isTrustedPluginStoreSource("https://store.floorp.app/plugin/demo", false),
    true,
  );
  assertEquals(
    isTrustedPluginStoreSource("https://plugins.floorp.app/", false),
    true,
  );
  assertEquals(
    isTrustedPluginStoreSource("https://store.floorp.app.evil.test/", false),
    false,
  );
  assertEquals(
    isTrustedPluginStoreSource("http://store.floorp.app/", false),
    false,
  );
}

function testDevelopmentOriginsAreExplicit(): void {
  assertEquals(
    isTrustedPluginStoreSource("http://localhost:5173/plugin", true),
    true,
  );
  assertEquals(
    isTrustedPluginStoreSource("http://127.0.0.1:5173/plugin", true),
    true,
  );
  assertEquals(
    isTrustedPluginStoreSource("http://localhost:5173/plugin", false),
    false,
  );
  assertEquals(
    isTrustedPluginStoreSource("file://localhost/plugin", true),
    false,
  );
}

function testPluginIdValidation(): void {
  assertEquals(isValidPluginId("official.demo-plugin"), true);
  assertEquals(isValidPluginId(""), false);
  assertEquals(isValidPluginId("bad\nplugin"), false);
  assertEquals(isValidPluginId("x".repeat(513)), false);
}

function testMetadataValidation(): void {
  assertEquals(
    isValidPluginMetadata({
      name: "Demo",
      description: "A demo plugin",
      author: "Floorp",
      isOfficial: true,
      functions: [{ name: "run", description: "Run" }],
    }),
    true,
  );
  assertEquals(isValidPluginMetadata({ name: "x".repeat(513) }), false);
  assertEquals(isValidPluginMetadata({ isOfficial: "yes" }), false);
  assertEquals(isValidPluginMetadata({ functions: {} }), false);
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "accepts only exact production origins",
      fn: testProductionOrigins,
    },
    {
      name: "allows loopback sources only in development",
      fn: testDevelopmentOriginsAreExplicit,
    },
    { name: "validates plugin identifiers", fn: testPluginIdValidation },
    { name: "bounds plugin metadata", fn: testMetadataValidation },
  ];
  await runTests("PluginStore Policy.test.mts", tests);
}
