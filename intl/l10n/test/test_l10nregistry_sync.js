/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  L10nRegistry,
} = ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm");
const {setTimeout} = ChromeUtils.import("resource://gre/modules/Timer.jsm");

add_task(function test_methods_presence() {
  equal(typeof L10nRegistry.generateBundles, "function");
  equal(typeof L10nRegistry.getAvailableLocales, "function");
  equal(typeof L10nRegistry.registerSources, "function");
  equal(typeof L10nRegistry.removeSources, "function");
  equal(typeof L10nRegistry.updateSources, "function");
});

/**
 * Test that passing empty resourceIds list works.
 */
add_task(function test_empty_resourceids() {
  const fs = [];

  const source = L10nFileSource.createMock("test", ["en-US"], "/localization/{locale}", fs);
  L10nRegistry.registerSources([source]);

  const bundles = L10nRegistry.generateBundlesSync(["en-US"], []);

  const done = (bundles.next()).done;

  equal(done, true);

  // cleanup
  L10nRegistry.sources.clear();
});

/**
 * Test that passing empty sources list works.
 */
add_task(function test_empty_sources() {
  const fs = [];
  const bundles = L10nRegistry.generateBundlesSync(["en-US"], fs);

  const done = (bundles.next()).done;

  equal(done, true);

  // cleanup
  L10nRegistry.sources.clear();
});

/**
 * This test tests generation of a proper context for a single
 * source scenario
 */
add_task(function test_methods_calling() {
  const fs = [
    { path: "/localization/en-US/browser/menu.ftl", source: "key = Value" }
  ];
  const source = L10nFileSource.createMock("test", ["en-US"], "/localization/{locale}", fs);
  L10nRegistry.registerSources([source]);

  const bundles = L10nRegistry.generateBundlesSync(["en-US"], ["/browser/menu.ftl"]);

  const bundle = (bundles.next()).value;

  equal(bundle.hasMessage("key"), true);

  // cleanup
  L10nRegistry.sources.clear();
});

/**
 * This test verifies that the public methods return expected values
 * for the single source scenario
 */
add_task(function test_has_one_source() {
  const fs = [
    {path: "./app/data/locales/en-US/test.ftl", source: "key = value en-US"}
  ];
  let oneSource = L10nFileSource.createMock("app", ["en-US"], "./app/data/locales/{locale}/", fs);
  L10nRegistry.registerSources([oneSource]);


  // has one source

  equal(L10nRegistry.sources.size, 1);
  equal(L10nRegistry.sources.has("app"), true);


  // returns a single context

  let bundles = L10nRegistry.generateBundlesSync(["en-US"], ["test.ftl"]);
  let bundle0 = (bundles.next()).value;
  equal(bundle0.hasMessage("key"), true);

  equal((bundles.next()).done, true);


  // returns no contexts for missing locale

  bundles = L10nRegistry.generateBundlesSync(["pl"], ["test.ftl"]);

  equal((bundles.next()).done, true);

  // cleanup
  L10nRegistry.sources.clear();
});

/**
 * This test verifies that public methods return expected values
 * for the dual source scenario.
 */
add_task(function test_has_two_sources() {
  const fs = [
    { path: "./platform/data/locales/en-US/test.ftl", source: "key = platform value" },
    { path: "./app/data/locales/pl/test.ftl", source: "key = app value" }
  ];
  let oneSource = L10nFileSource.createMock("platform", ["en-US"], "./platform/data/locales/{locale}/", fs);
  let secondSource = L10nFileSource.createMock("app", ["pl"], "./app/data/locales/{locale}/", fs);
  L10nRegistry.registerSources([oneSource, secondSource]);

  // has two sources

  equal(L10nRegistry.sources.size, 2);
  equal(L10nRegistry.sources.has("app"), true);
  equal(L10nRegistry.sources.has("platform"), true);


  // returns correct contexts for en-US

  let bundles = L10nRegistry.generateBundlesSync(["en-US"], ["test.ftl"]);
  let bundle0 = (bundles.next()).value;

  equal(bundle0.hasMessage("key"), true);
  let msg = bundle0.getMessage("key");
  equal(bundle0.formatPattern(msg.value), "platform value");

  equal((bundles.next()).done, true);


  // returns correct contexts for [pl, en-US]

  bundles = L10nRegistry.generateBundlesSync(["pl", "en-US"], ["test.ftl"]);
  bundle0 = (bundles.next()).value;
  equal(bundle0.locales[0], "pl");
  equal(bundle0.hasMessage("key"), true);
  let msg0 = bundle0.getMessage("key");
  equal(bundle0.formatPattern(msg0.value), "app value");

  let bundle1 = (bundles.next()).value;
  equal(bundle1.locales[0], "en-US");
  equal(bundle1.hasMessage("key"), true);
  let msg1 = bundle1.getMessage("key");
  equal(bundle1.formatPattern(msg1.value), "platform value");

  equal((bundles.next()).done, true);

  // cleanup
  L10nRegistry.sources.clear();
});

/**
 * This test checks if the correct order of contexts is used for
 * scenarios where a new file source is added on top of the default one.
 */
add_task(function test_override() {
  const fs = [
    { path: "/app/data/locales/pl/test.ftl", source: "key = value" },
    { path: "/data/locales/pl/test.ftl", source: "key = addon value"},
  ];
  let fileSource = L10nFileSource.createMock("app", ["pl"], "/app/data/locales/{locale}/", fs);
  let oneSource = L10nFileSource.createMock("langpack-pl", ["pl"], "/data/locales/{locale}/", fs);
  L10nRegistry.registerSources([fileSource, oneSource]);

  equal(L10nRegistry.sources.size, 2);
  equal(L10nRegistry.sources.has("langpack-pl"), true);

  let bundles = L10nRegistry.generateBundlesSync(["pl"], ["test.ftl"]);
  let bundle0 = (bundles.next()).value;
  equal(bundle0.locales[0], "pl");
  equal(bundle0.hasMessage("key"), true);
  let msg0 = bundle0.getMessage("key");
  equal(bundle0.formatPattern(msg0.value), "addon value");

  let bundle1 = (bundles.next()).value;
  equal(bundle1.locales[0], "pl");
  equal(bundle1.hasMessage("key"), true);
  let msg1 = bundle1.getMessage("key");
  equal(bundle1.formatPattern(msg1.value), "value");

  equal((bundles.next()).done, true);

  // cleanup
  L10nRegistry.sources.clear();
});

/**
 * This test verifies that new contexts are returned
 * after source update.
 */
add_task(function test_updating() {
  const fs = [
    { path: "/data/locales/pl/test.ftl", source: "key = value" }
  ];
  let oneSource = L10nFileSource.createMock("langpack-pl", ["pl"], "/data/locales/{locale}/", fs);
  L10nRegistry.registerSources([oneSource]);

  let bundles = L10nRegistry.generateBundlesSync(["pl"], ["test.ftl"]);
  let bundle0 = (bundles.next()).value;
  equal(bundle0.locales[0], "pl");
  equal(bundle0.hasMessage("key"), true);
  let msg0 = bundle0.getMessage("key");
  equal(bundle0.formatPattern(msg0.value), "value");


  const newSource = L10nFileSource.createMock("langpack-pl", ["pl"], "/data/locales/{locale}/", [
    { path: "/data/locales/pl/test.ftl", source: "key = new value" }
  ]);
  L10nRegistry.updateSources([newSource]);

  equal(L10nRegistry.sources.size, 1);
  bundles = L10nRegistry.generateBundlesSync(["pl"], ["test.ftl"]);
  bundle0 = (bundles.next()).value;
  msg0 = bundle0.getMessage("key");
  equal(bundle0.formatPattern(msg0.value), "new value");

  // cleanup
  L10nRegistry.sources.clear();
});

/**
 * This test verifies that generated contexts return correct values
 * after sources are being removed.
 */
add_task(function test_removing() {
  const fs = [
    { path: "/app/data/locales/pl/test.ftl", source: "key = value" },
    { path: "/data/locales/pl/test.ftl", source: "key = addon value" },
  ];

  let fileSource = L10nFileSource.createMock("app", ["pl"], "/app/data/locales/{locale}/", fs);
  let oneSource = L10nFileSource.createMock("langpack-pl", ["pl"], "/data/locales/{locale}/", fs);

  L10nRegistry.registerSources([fileSource, oneSource]);

  equal(L10nRegistry.sources.size, 2);
  equal(L10nRegistry.sources.has("langpack-pl"), true);

  let bundles = L10nRegistry.generateBundlesSync(["pl"], ["test.ftl"]);
  let bundle0 = (bundles.next()).value;
  equal(bundle0.locales[0], "pl");
  equal(bundle0.hasMessage("key"), true);
  let msg0 = bundle0.getMessage("key");
  equal(bundle0.formatPattern(msg0.value), "addon value");

  let bundle1 = (bundles.next()).value;
  equal(bundle1.locales[0], "pl");
  equal(bundle1.hasMessage("key"), true);
  let msg1 = bundle1.getMessage("key");
  equal(bundle1.formatPattern(msg1.value), "value");

  equal((bundles.next()).done, true);

  // Remove langpack

  L10nRegistry.removeSources(["langpack-pl"]);

  equal(L10nRegistry.sources.size, 1);
  equal(L10nRegistry.sources.has("langpack-pl"), false);

  bundles = L10nRegistry.generateBundlesSync(["pl"], ["test.ftl"]);
  bundle0 = (bundles.next()).value;
  equal(bundle0.locales[0], "pl");
  equal(bundle0.hasMessage("key"), true);
  msg0 = bundle0.getMessage("key");
  equal(bundle0.formatPattern(msg0.value), "value");

  equal((bundles.next()).done, true);

  // Remove app source

  L10nRegistry.removeSources(["app"]);

  equal(L10nRegistry.sources.size, 0);

  bundles = L10nRegistry.generateBundlesSync(["pl"], ["test.ftl"]);
  equal((bundles.next()).done, true);

  // cleanup
  L10nRegistry.sources.clear();
});

/**
 * This test verifies that the logic works correctly when there's a missing
 * file in the FileSource scenario.
 */
add_task(function test_missing_file() {
  const fs = [
    { path: "./app/data/locales/en-US/test.ftl", source: "key = value en-US" },
    { path: "./platform/data/locales/en-US/test.ftl", source: "key = value en-US" },
    { path: "./platform/data/locales/en-US/test2.ftl", source: "key2 = value2 en-US" },
  ];
  let oneSource = L10nFileSource.createMock("app", ["en-US"], "./app/data/locales/{locale}/", fs);
  let twoSource = L10nFileSource.createMock("platform", ["en-US"], "./platform/data/locales/{locale}/", fs);
  L10nRegistry.registerSources([oneSource, twoSource]);

  // has two sources

  equal(L10nRegistry.sources.size, 2);
  equal(L10nRegistry.sources.has("app"), true);
  equal(L10nRegistry.sources.has("platform"), true);


  // returns a single context

  let bundles = L10nRegistry.generateBundlesSync(["en-US"], ["test.ftl", "test2.ftl"]);

  // First permutation:
  //   [platform, platform] - both present
  let bundle1 = (bundles.next());
  equal(bundle1.value.hasMessage("key"), true);

  // Second permutation skipped:
  //   [platform, app] - second missing
  // Third permutation:
  //   [app, platform] - both present
  let bundle2 = (bundles.next());
  equal(bundle2.value.hasMessage("key"), true);

  // Fourth permutation skipped:
  //   [app, app] - second missing
  equal((bundles.next()).done, true);

  // cleanup
  L10nRegistry.sources.clear();
});

/**
 * This test verifies that we handle correctly a scenario where a source
 * is being removed while the iterator operates.
 */
add_task(function test_remove_source_mid_iter_cycle() {
  const fs = [
    { path: "./platform/data/locales/en-US/test.ftl", source: "key = platform value" },
    { path: "./app/data/locales/pl/test.ftl", source: "key = app value" },
  ];
  let oneSource = L10nFileSource.createMock("platform", ["en-US"], "./platform/data/locales/{locale}/", fs);
  let secondSource = L10nFileSource.createMock("app", ["pl"], "./app/data/locales/{locale}/", fs);
  L10nRegistry.registerSources([oneSource, secondSource]);

  let bundles = L10nRegistry.generateBundlesSync(["en-US", "pl"], ["test.ftl"]);

  let bundle0 = bundles.next();

  L10nRegistry.removeSources(["app"]);

  equal((bundles.next()).done, true);

  // cleanup
  L10nRegistry.sources.clear();
});
