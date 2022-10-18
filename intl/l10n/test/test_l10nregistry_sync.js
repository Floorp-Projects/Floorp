/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {setTimeout} = ChromeUtils.importESModule("resource://gre/modules/Timer.sys.mjs");

const l10nReg = new L10nRegistry();

add_task(function test_methods_presence() {
  equal(typeof l10nReg.generateBundles, "function");
  equal(typeof l10nReg.generateBundlesSync, "function");
  equal(typeof l10nReg.getAvailableLocales, "function");
  equal(typeof l10nReg.registerSources, "function");
  equal(typeof l10nReg.removeSources, "function");
  equal(typeof l10nReg.updateSources, "function");
});

/**
 * Test that passing empty resourceIds list works.
 */
add_task(function test_empty_resourceids() {
  const fs = [];

  const source = L10nFileSource.createMock("test", "", ["en-US"], "/localization/{locale}", fs);
  l10nReg.registerSources([source]);

  const bundles = l10nReg.generateBundlesSync(["en-US"], []);

  const done = (bundles.next()).done;

  equal(done, true);

  // cleanup
  l10nReg.clearSources();
});

/**
 * Test that passing empty sources list works.
 */
add_task(function test_empty_sources() {
  const fs = [];
  const bundles = l10nReg.generateBundlesSync(["en-US"], fs);

  const done = (bundles.next()).done;

  equal(done, true);

  // cleanup
  l10nReg.clearSources();
});

/**
 * This test tests generation of a proper context for a single
 * source scenario
 */
add_task(function test_methods_calling() {
  const fs = [
    { path: "/localization/en-US/browser/menu.ftl", source: "key = Value" }
  ];
  const source = L10nFileSource.createMock("test", "", ["en-US"], "/localization/{locale}", fs);
  l10nReg.registerSources([source]);

  const bundles = l10nReg.generateBundlesSync(["en-US"], ["/browser/menu.ftl"]);

  const bundle = (bundles.next()).value;

  equal(bundle.hasMessage("key"), true);

  // cleanup
  l10nReg.clearSources();
});

/**
 * This test verifies that the public methods return expected values
 * for the single source scenario
 */
add_task(function test_has_one_source() {
  const fs = [
    {path: "./app/data/locales/en-US/test.ftl", source: "key = value en-US"}
  ];
  let oneSource = L10nFileSource.createMock("app", "", ["en-US"], "./app/data/locales/{locale}/", fs);
  l10nReg.registerSources([oneSource]);


  // has one source

  equal(l10nReg.getSourceNames().length, 1);
  equal(l10nReg.hasSource("app"), true);


  // returns a single context

  let bundles = l10nReg.generateBundlesSync(["en-US"], ["test.ftl"]);
  let bundle0 = (bundles.next()).value;
  equal(bundle0.hasMessage("key"), true);

  equal((bundles.next()).done, true);


  // returns no contexts for missing locale

  bundles = l10nReg.generateBundlesSync(["pl"], ["test.ftl"]);

  equal((bundles.next()).done, true);

  // cleanup
  l10nReg.clearSources();
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
  let oneSource = L10nFileSource.createMock("platform", "", ["en-US"], "./platform/data/locales/{locale}/", fs);
  let secondSource = L10nFileSource.createMock("app", "", ["pl"], "./app/data/locales/{locale}/", fs);
  l10nReg.registerSources([oneSource, secondSource]);

  // has two sources

  equal(l10nReg.getSourceNames().length, 2);
  equal(l10nReg.hasSource("app"), true);
  equal(l10nReg.hasSource("platform"), true);


  // returns correct contexts for en-US

  let bundles = l10nReg.generateBundlesSync(["en-US"], ["test.ftl"]);
  let bundle0 = (bundles.next()).value;

  equal(bundle0.hasMessage("key"), true);
  let msg = bundle0.getMessage("key");
  equal(bundle0.formatPattern(msg.value), "platform value");

  equal((bundles.next()).done, true);


  // returns correct contexts for [pl, en-US]

  bundles = l10nReg.generateBundlesSync(["pl", "en-US"], ["test.ftl"]);
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
  l10nReg.clearSources();
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
  let fileSource = L10nFileSource.createMock("app", "", ["pl"], "/app/data/locales/{locale}/", fs);
  let oneSource = L10nFileSource.createMock("langpack-pl", "", ["pl"], "/data/locales/{locale}/", fs);
  l10nReg.registerSources([fileSource, oneSource]);

  equal(l10nReg.getSourceNames().length, 2);
  equal(l10nReg.hasSource("langpack-pl"), true);

  let bundles = l10nReg.generateBundlesSync(["pl"], ["test.ftl"]);
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
  l10nReg.clearSources();
});

/**
 * This test verifies that new contexts are returned
 * after source update.
 */
add_task(function test_updating() {
  const fs = [
    { path: "/data/locales/pl/test.ftl", source: "key = value" }
  ];
  let oneSource = L10nFileSource.createMock("langpack-pl", "", ["pl"], "/data/locales/{locale}/", fs);
  l10nReg.registerSources([oneSource]);

  let bundles = l10nReg.generateBundlesSync(["pl"], ["test.ftl"]);
  let bundle0 = (bundles.next()).value;
  equal(bundle0.locales[0], "pl");
  equal(bundle0.hasMessage("key"), true);
  let msg0 = bundle0.getMessage("key");
  equal(bundle0.formatPattern(msg0.value), "value");


  const newSource = L10nFileSource.createMock("langpack-pl", "", ["pl"], "/data/locales/{locale}/", [
    { path: "/data/locales/pl/test.ftl", source: "key = new value" }
  ]);
  l10nReg.updateSources([newSource]);

  equal(l10nReg.getSourceNames().length, 1);
  bundles = l10nReg.generateBundlesSync(["pl"], ["test.ftl"]);
  bundle0 = (bundles.next()).value;
  msg0 = bundle0.getMessage("key");
  equal(bundle0.formatPattern(msg0.value), "new value");

  // cleanup
  l10nReg.clearSources();
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

  let fileSource = L10nFileSource.createMock("app", "", ["pl"], "/app/data/locales/{locale}/", fs);
  let oneSource = L10nFileSource.createMock("langpack-pl", "", ["pl"], "/data/locales/{locale}/", fs);
  l10nReg.registerSources([fileSource, oneSource]);

  equal(l10nReg.getSourceNames().length, 2);
  equal(l10nReg.hasSource("langpack-pl"), true);

  let bundles = l10nReg.generateBundlesSync(["pl"], ["test.ftl"]);
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

  l10nReg.removeSources(["langpack-pl"]);

  equal(l10nReg.getSourceNames().length, 1);
  equal(l10nReg.hasSource("langpack-pl"), false);

  bundles = l10nReg.generateBundlesSync(["pl"], ["test.ftl"]);
  bundle0 = (bundles.next()).value;
  equal(bundle0.locales[0], "pl");
  equal(bundle0.hasMessage("key"), true);
  msg0 = bundle0.getMessage("key");
  equal(bundle0.formatPattern(msg0.value), "value");

  equal((bundles.next()).done, true);

  // Remove app source

  l10nReg.removeSources(["app"]);

  equal(l10nReg.getSourceNames().length, 0);

  bundles = l10nReg.generateBundlesSync(["pl"], ["test.ftl"]);
  equal((bundles.next()).done, true);

  // cleanup
  l10nReg.clearSources();
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
  let oneSource = L10nFileSource.createMock("app", "", ["en-US"], "./app/data/locales/{locale}/", fs);
  let twoSource = L10nFileSource.createMock("platform", "", ["en-US"], "./platform/data/locales/{locale}/", fs);
  l10nReg.registerSources([oneSource, twoSource]);

  // has two sources

  equal(l10nReg.getSourceNames().length, 2);
  equal(l10nReg.hasSource("app"), true);
  equal(l10nReg.hasSource("platform"), true);


  // returns a single context

  let bundles = l10nReg.generateBundlesSync(["en-US"], ["test.ftl", "test2.ftl"]);

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
  l10nReg.clearSources();
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
  let oneSource = L10nFileSource.createMock("platform", "", ["en-US"], "./platform/data/locales/{locale}/", fs);
  let secondSource = L10nFileSource.createMock("app", "", ["pl"], "./app/data/locales/{locale}/", fs);
  l10nReg.registerSources([oneSource, secondSource]);

  let bundles = l10nReg.generateBundlesSync(["en-US", "pl"], ["test.ftl"]);

  let bundle0 = bundles.next();

  l10nReg.removeSources(["app"]);

  equal((bundles.next()).done, true);

  // cleanup
  l10nReg.clearSources();
});

add_task(async function test_metasources() {
  let fs = [
    { path: "/localization/en-US/browser/menu1.ftl", source: "key1 = Value" },
    { path: "/localization/en-US/browser/menu2.ftl", source: "key2 = Value" },
    { path: "/localization/en-US/browser/menu3.ftl", source: "key3 = Value" },
    { path: "/localization/en-US/browser/menu4.ftl", source: "key4 = Value" },
    { path: "/localization/en-US/browser/menu5.ftl", source: "key5 = Value" },
    { path: "/localization/en-US/browser/menu6.ftl", source: "key6 = Value" },
    { path: "/localization/en-US/browser/menu7.ftl", source: "key7 = Value" },
    { path: "/localization/en-US/browser/menu8.ftl", source: "key8 = Value" },
  ];

  const browser = L10nFileSource.createMock("browser", "app", ["en-US"], "/localization/{locale}", fs);
  const toolkit = L10nFileSource.createMock("toolkit", "app", ["en-US"], "/localization/{locale}", fs);
  const browser2 = L10nFileSource.createMock("browser2", "langpack", ["en-US"], "/localization/{locale}", fs);
  const toolkit2 = L10nFileSource.createMock("toolkit2", "langpack", ["en-US"], "/localization/{locale}", fs);
  l10nReg.registerSources([toolkit, browser, toolkit2, browser2]);

  let res = [
    "/browser/menu1.ftl",
    "/browser/menu2.ftl",
    "/browser/menu3.ftl",
    "/browser/menu4.ftl",
    "/browser/menu5.ftl",
    "/browser/menu6.ftl",
    "/browser/menu7.ftl",
    {path: "/browser/menu8.ftl", optional: false},
  ];

  const bundles = l10nReg.generateBundlesSync(["en-US"], res);

  let nbundles = 0;
  while (!bundles.next().done) {
    nbundles += 1;
  }

  // If metasources are working properly, we'll generate 2^8 = 256 bundles for
  // each metasource giving 512 bundles in total. Otherwise, we generate
  // 4^8 = 65536 bundles.
  equal(nbundles, 512);

  // cleanup
  l10nReg.clearSources();
});

/**
 * This test verifies that when a required resource is missing for a locale,
 * we do not produce a bundle for that locale.
 */
add_task(function test_missing_required_resource() {
  const fs = [
    { path: "./platform/data/locales/en-US/test.ftl", source: "test-key = en-US value" },
    { path: "./platform/data/locales/pl/missing-in-en-US.ftl", source: "missing-key = pl value" },
    { path: "./platform/data/locales/pl/test.ftl", source: "test-key = pl value" },
  ];
  let source = L10nFileSource.createMock("platform", "", ["en-US", "pl"], "./platform/data/locales/{locale}/", fs);
  l10nReg.registerSources([source]);

  equal(l10nReg.getSourceNames().length, 1);
  equal(l10nReg.hasSource("platform"), true);


  // returns correct contexts for [en-US, pl]

  let bundles = l10nReg.generateBundlesSync(["en-US", "pl"], ["test.ftl", "missing-in-en-US.ftl"]);
  let bundle0 = (bundles.next()).value;

  equal(bundle0.locales[0], "pl");
  equal(bundle0.hasMessage("test-key"), true);
  equal(bundle0.hasMessage("missing-key"), true);

  let msg0 = bundle0.getMessage("test-key");
  equal(bundle0.formatPattern(msg0.value), "pl value");

  let msg1 = bundle0.getMessage("missing-key");
  equal(bundle0.formatPattern(msg1.value), "pl value");

  equal((bundles.next()).done, true);


  // returns correct contexts for [pl, en-US]

  bundles = l10nReg.generateBundlesSync(["pl", "en-US"], ["test.ftl", {path: "missing-in-en-US.ftl", optional: false}]);
  bundle0 = (bundles.next()).value;

  equal(bundle0.locales[0], "pl");
  equal(bundle0.hasMessage("test-key"), true);

  msg0 = bundle0.getMessage("test-key");
  equal(bundle0.formatPattern(msg0.value), "pl value");

  msg1 = bundle0.getMessage("missing-key");
  equal(bundle0.formatPattern(msg1.value), "pl value");

  equal((bundles.next()).done, true);

  // cleanup
  l10nReg.clearSources();
});

/**
 * This test verifies that when an optional resource is missing, we continue
 * to produce a bundle for that locale. The bundle will have missing entries
 * with regard to the missing optional resource.
 */
add_task(function test_missing_optional_resource() {
  const fs = [
    { path: "./platform/data/locales/en-US/test.ftl", source: "test-key = en-US value" },
    { path: "./platform/data/locales/pl/missing-in-en-US.ftl", source: "missing-key = pl value" },
    { path: "./platform/data/locales/pl/test.ftl", source: "test-key = pl value" },
  ];
  let source = L10nFileSource.createMock("platform", "", ["en-US", "pl"], "./platform/data/locales/{locale}/", fs);
  l10nReg.registerSources([source]);

  equal(l10nReg.getSourceNames().length, 1);
  equal(l10nReg.hasSource("platform"), true);


  // returns correct contexts for [en-US, pl]

  let bundles = l10nReg.generateBundlesSync(["en-US", "pl"], ["test.ftl", { path: "missing-in-en-US.ftl", optional: true }]);
  let bundle0 = (bundles.next()).value;

  equal(bundle0.locales[0], "en-US");
  equal(bundle0.hasMessage("test-key"), true);
  equal(bundle0.hasMessage("missing-key"), false);

  let msg0 = bundle0.getMessage("test-key");
  equal(bundle0.formatPattern(msg0.value), "en-US value");

  equal(bundle0.getMessage("missing-key"), null);

  let bundle1 = (bundles.next()).value;

  equal(bundle1.locales[0], "pl");
  equal(bundle1.hasMessage("test-key"), true);
  equal(bundle1.hasMessage("missing-key"), true);

  msg0 = bundle1.getMessage("test-key");
  equal(bundle1.formatPattern(msg0.value), "pl value");

  msg1 = bundle1.getMessage("missing-key");
  equal(bundle1.formatPattern(msg1.value), "pl value");

  equal((bundles.next()).done, true);

  // returns correct contexts for [pl, en-US]

  bundles = l10nReg.generateBundlesSync(["pl", "en-US"], ["test.ftl", { path: "missing-in-en-US.ftl", optional: true }]);
  bundle0 = (bundles.next()).value;

  equal(bundle0.locales[0], "pl");
  equal(bundle0.hasMessage("test-key"), true);
  equal(bundle0.hasMessage("missing-key"), true);

  msg0 = bundle0.getMessage("test-key");
  equal(bundle0.formatPattern(msg0.value), "pl value");

  msg1 = bundle0.getMessage("missing-key");
  equal(bundle0.formatPattern(msg1.value), "pl value");

  bundle1 = (bundles.next()).value;

  equal(bundle1.locales[0], "en-US");
  equal(bundle1.hasMessage("test-key"), true);
  equal(bundle1.hasMessage("missing-key"), false);

  msg0 = bundle1.getMessage("test-key");
  equal(bundle1.formatPattern(msg0.value), "en-US value");

  equal(bundle1.getMessage("missing-key"), null);

  // cleanup
  l10nReg.clearSources();
});
