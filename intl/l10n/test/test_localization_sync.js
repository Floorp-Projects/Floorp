/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

add_task(function test_methods_calling() {
  const l10nReg = new L10nRegistry();

  const fs = [
    { path: "/localization/de/browser/menu.ftl", source: `
key-value1 = [de] Value2
` },
    { path: "/localization/en-US/browser/menu.ftl", source: `
key-value1 = [en] Value2
key-value2 = [en] Value3
key-attr =
    .label = [en] Label 3
` },
  ];

  const source = L10nFileSource.createMock("test", "app", ["de", "en-US"], "/localization/{locale}", fs);
  l10nReg.registerSources([source]);

  const l10n = new Localization([
    "/browser/menu.ftl",
  ], true, l10nReg, ["de", "en-US"]);


  {
    let values = l10n.formatValuesSync([
      {id: "key-value1"},
      {id: "key-value2"},
      {id: "key-missing"},
      {id: "key-attr"}
    ]);

    strictEqual(values[0], "[de] Value2");
    strictEqual(values[1], "[en] Value3");
    strictEqual(values[2], null);
    strictEqual(values[3], null);
  }

  {
    let values = l10n.formatValuesSync([
      "key-value1",
      "key-value2",
      "key-missing",
      "key-attr"
    ]);

    strictEqual(values[0], "[de] Value2");
    strictEqual(values[1], "[en] Value3");
    strictEqual(values[2], null);
    strictEqual(values[3], null);
  }

  {
    strictEqual(l10n.formatValueSync("key-missing"), null);
    strictEqual(l10n.formatValueSync("key-value1"), "[de] Value2");
    strictEqual(l10n.formatValueSync("key-value2"), "[en] Value3");
    strictEqual(l10n.formatValueSync("key-attr"), null);
  }

  {
    let messages = l10n.formatMessagesSync([
      {id: "key-value1"},
      {id: "key-missing"},
      {id: "key-value2"},
      {id: "key-attr"},
    ]);

    strictEqual(messages[0].value, "[de] Value2");
    strictEqual(messages[1], null);
    strictEqual(messages[2].value, "[en] Value3");
    strictEqual(messages[3].value, null);
  }
});

add_task(function test_builtins() {
  const known_platforms = {
    "linux": "linux",
    "win": "windows",
    "macosx": "macos",
    "android": "android",
  };

  const fs = [
    { path: "/localization/en-US/test.ftl", source: `
key = { PLATFORM() ->
        ${ Object.values(known_platforms).map(
              name => `      [${ name }] ${ name.toUpperCase() } Value\n`).join("") }
       *[other] OTHER Value
    }` },
  ];

  const source = L10nFileSource.createMock("test", "app", ["en-US"], "/localization/{locale}", fs);
  const l10nReg = new L10nRegistry();
  l10nReg.registerSources([source]);

  const l10n = new Localization([
    "/test.ftl",
  ], true, l10nReg, ["en-US"]);

  let values = l10n.formatValuesSync([{id: "key"}]);

  ok(values[0].includes(
    `${ known_platforms[AppConstants.platform].toUpperCase() } Value`));
});

add_task(function test_add_remove_resourceIds() {
  const fs = [
    { path: "/localization/en-US/browser/menu.ftl", source: "key1 = Value1" },
    { path: "/localization/en-US/toolkit/menu.ftl", source: "key2 = Value2" },
  ];
  const originalRequested = Services.locale.requestedLocales;

  const source = L10nFileSource.createMock("test", "app", ["en-US"], "/localization/{locale}", fs);
  const l10nReg = new L10nRegistry();
  l10nReg.registerSources([source]);

  const l10n = new Localization(["/browser/menu.ftl"], true, l10nReg, ["en-US"]);

  let values = l10n.formatValuesSync([{id: "key1"}, {id: "key2"}]);

  strictEqual(values[0], "Value1");
  strictEqual(values[1], null);

  l10n.addResourceIds(["/toolkit/menu.ftl"]);

  values = l10n.formatValuesSync([{id: "key1"}, {id: "key2"}]);

  strictEqual(values[0], "Value1");
  strictEqual(values[1], "Value2");

  values = l10n.formatValuesSync(["key1", {id: "key2"}]);

  strictEqual(values[0], "Value1");
  strictEqual(values[1], "Value2");

  values = l10n.formatValuesSync([{id: "key1"}, "key2"]);

  strictEqual(values[0], "Value1");
  strictEqual(values[1], "Value2");

  l10n.removeResourceIds(["/browser/menu.ftl"]);

  values = l10n.formatValuesSync([{id: "key1"}, {id: "key2"}]);

  strictEqual(values[0], null);
  strictEqual(values[1], "Value2");
});

add_task(function test_calling_sync_methods_in_async_mode_fails() {
  const l10n = new Localization(["/browser/menu.ftl"], false);

  Assert.throws(() => {
    l10n.formatValuesSync([{ id: "key1" }, { id: "key2" }]);
  }, /Can't use formatValuesSync when state is async./);

  Assert.throws(() => {
    l10n.formatValueSync("key1");
  }, /Can't use formatValueSync when state is async./);

  Assert.throws(() => {
    l10n.formatMessagesSync([{ id: "key1"}]);
  }, /Can't use formatMessagesSync when state is async./);
});

/**
 * This test verifies that when a required resource is missing,
 * we fallback entirely to the next locale for all entries.
 */
add_task(function test_format_from_missing_required_resource() {
  const l10nReg = new L10nRegistry();

  const fs = [
    { path: "/localization/de/browser/menu.ftl", source: `
key-value = [de] Value1
` },
    { path: "/localization/de/browser/missing-in-en-US.ftl", source: `
key-missing = [de] MissingValue
` },
    { path: "/localization/en-US/browser/menu.ftl", source: `
key-value = [en] Value1
` },
  ];

  const source = L10nFileSource.createMock("test", "app", ["de", "en-US"], "/localization/{locale}", fs);
  l10nReg.registerSources([source]);

  // returns correct contexts for [en-US, de]

  let l10n = new Localization([
    "/browser/menu.ftl",
    "/browser/missing-in-en-US.ftl",
  ], true, l10nReg, ["en-US", "de"]);

  {
    let values = l10n.formatValuesSync([
      {id: "key-value"},
      {id: "key-missing"},
    ]);

    strictEqual(values[0], "[de] Value1");
    strictEqual(values[1], "[de] MissingValue");
  }

  // returns correct contexts for [de, en-US]

  l10n = new Localization([
    "/browser/menu.ftl",
    {path: "/browser/missing-in-en-US.ftl", optional: false},
  ], true, l10nReg, ["de", "en-US"]);

  {
    let values = l10n.formatValuesSync([
      {id: "key-value"},
      {id: "key-missing"},
    ]);

    strictEqual(values[0], "[de] Value1");
    strictEqual(values[1], "[de] MissingValue");
  }
});

/**
 * This test verifies that when an optional resource is missing
 * we continue to populate entires from other resources in the same locale
 * and only fallback entries from the optional resource to the next locale.
 */
add_task(function test_format_from_missing_optional_resource() {
  const l10nReg = new L10nRegistry();

  const fs = [
    { path: "/localization/de/browser/menu.ftl", source: `
key-value = [de] Value1
` },
    { path: "/localization/de/browser/missing-in-en-US.ftl", source: `
key-missing = [de] MissingValue
` },
    { path: "/localization/en-US/browser/menu.ftl", source: `
key-value = [en] Value1
` },
  ];

  const source = L10nFileSource.createMock("test", "app", ["de", "en-US"], "/localization/{locale}", fs);
  l10nReg.registerSources([source]);

  // returns correct contexts for [en-US, de]

  let l10n = new Localization([
    {path: "/browser/menu.ftl", optional: false},
    {path: "/browser/missing-in-en-US.ftl", optional: true},
  ], true, l10nReg, ["en-US", "de"]);

  {
    let values = l10n.formatValuesSync([
      {id: "key-value"},
      {id: "key-missing"},
    ]);

    strictEqual(values[0], "[en] Value1");
    strictEqual(values[1], "[de] MissingValue");
  }

  // returns correct contexts for [de, en-US]

  l10n = new Localization([
    {path: "/browser/menu.ftl", optional: false},
    {path: "/browser/missing-in-en-US.ftl", optional: true},
  ], true, l10nReg, ["de", "en-US"]);

  {
    let values = l10n.formatValuesSync([
      {id: "key-value"},
      {id: "key-missing"},
    ]);

    strictEqual(values[0], "[de] Value1");
    strictEqual(values[1], "[de] MissingValue");
  }
});
