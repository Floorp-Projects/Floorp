var BrandingRegression = (() => {
  var __defProp = Object.defineProperty;
  var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
  var __getOwnPropNames = Object.getOwnPropertyNames;
  var __hasOwnProp = Object.prototype.hasOwnProperty;
  var __export = (target, all) => {
    for (var name in all)
      __defProp(target, name, { get: all[name], enumerable: true });
  };
  var __copyProps = (to, from, except, desc) => {
    if (from && typeof from === "object" || typeof from === "function") {
      for (let key of __getOwnPropNames(from))
        if (!__hasOwnProp.call(to, key) && key !== except)
          __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
    }
    return to;
  };
  var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

  // browser-features/modules/modules/i18n/test/LegacyBranding.test.mts
  var LegacyBranding_test_exports = {};
  __export(LegacyBranding_test_exports, {
    runAllTests: () => runAllTests
  });

  // browser-features/chrome/test/utils/test_harness.ts
  function assertEquals(actual, expected, message) {
    if (actual !== expected) {
      throw new Error(
        `${message} (expected: ${String(expected)}, actual: ${String(actual)})`
      );
    }
  }
  async function runTests(moduleName, tests) {
    const failures = [];
    for (const [index, test] of tests.entries()) {
      globalThis.__NORA_TEST_PROGRESS__ = {
        moduleName,
        testName: test.name,
        status: "running",
        index: index + 1,
        total: tests.length,
        startedAtMs: Date.now()
      };
      try {
        await test.fn();
        const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ?? Date.now();
        globalThis.__NORA_TEST_PROGRESS__ = {
          moduleName,
          testName: test.name,
          status: "passed",
          index: index + 1,
          total: tests.length,
          startedAtMs
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        failures.push(`${test.name}: ${message}`);
        const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ?? Date.now();
        globalThis.__NORA_TEST_PROGRESS__ = {
          moduleName,
          testName: test.name,
          status: "failed",
          index: index + 1,
          total: tests.length,
          startedAtMs
        };
      }
    }
    globalThis.__NORA_TEST_PROGRESS__ = {
      moduleName,
      testName: "",
      status: "done",
      index: tests.length,
      total: tests.length,
      startedAtMs: Date.now()
    };
    if (failures.length > 0) {
      throw new Error(`${moduleName} failures: ${failures.join(" | ")}`);
    }
  }

  // browser-features/modules/modules/i18n/test/LegacyBranding.test.mts
  async function runAllTests() {
    await runTests("LegacyBranding.test.mts", [
      {
        name: "legacy product names retain packaged branding in every UI locale",
        fn() {
          const packaged = Services.strings.createBundle(
            "resource:///chrome/en-US/locale/branding/brand.properties"
          );
          const effective = Services.strings.createBundle(
            "chrome://branding/locale/brand.properties"
          );
          for (const key of [
            "brandShorterName",
            "brandShortName",
            "brandFullName",
            "vendorShortName"
          ]) {
            assertEquals(
              effective.GetStringFromName(key),
              packaged.GetStringFromName(key),
              `${key} must not be replaced by an active Firefox language pack`
            );
          }
        }
      }
    ]);
  }
  return __toCommonJS(LegacyBranding_test_exports);
})();
