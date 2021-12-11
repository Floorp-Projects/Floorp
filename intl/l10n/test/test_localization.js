/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

add_task(function test_methods_presence() {
  strictEqual(typeof Localization.prototype.formatValues, "function");
  strictEqual(typeof Localization.prototype.formatMessages, "function");
  strictEqual(typeof Localization.prototype.formatValue, "function");
});

add_task(async function test_methods_calling() {
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
  const originalRequested = Services.locale.requestedLocales;

  const source = L10nFileSource.createMock("test", "app", ["de", "en-US"], "/localization/{locale}", fs);
  l10nReg.registerSources([source]);

  const l10n = new Localization([
    "/browser/menu.ftl",
  ], false, l10nReg, ["de", "en-US"]);

  {
    let values = await l10n.formatValues([
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
    let values = await l10n.formatValues([
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
    strictEqual(await l10n.formatValue("key-missing"), null);
    strictEqual(await l10n.formatValue("key-value1"), "[de] Value2");
    strictEqual(await l10n.formatValue("key-value2"), "[en] Value3");
    strictEqual(await l10n.formatValue("key-attr"), null);
  }

  {
    let messages = await l10n.formatMessages([
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

add_task(async function test_builtins() {
  const l10nReg = new L10nRegistry();
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
  l10nReg.registerSources([source]);

  const l10n = new Localization([
    "/test.ftl",
  ], false, l10nReg, ["en-US"]);

  let values = await l10n.formatValues([{id: "key"}]);

  ok(values[0].includes(
    `${ known_platforms[AppConstants.platform].toUpperCase() } Value`));
});

add_task(async function test_add_remove_resourceIds() {
  const l10nReg = new L10nRegistry();
  const fs = [
    { path: "/localization/en-US/browser/menu.ftl", source: "key1 = Value1" },
    { path: "/localization/en-US/toolkit/menu.ftl", source: "key2 = Value2" },
  ];


  const source = L10nFileSource.createMock("test", "app", ["en-US"], "/localization/{locale}", fs);
  l10nReg.registerSources([source]);

  const l10n = new Localization(["/browser/menu.ftl"], false, l10nReg, ["en-US"]);

  let values = await l10n.formatValues([{id: "key1"}, {id: "key2"}]);

  strictEqual(values[0], "Value1");
  strictEqual(values[1], null);

  l10n.addResourceIds(["/toolkit/menu.ftl"]);

  values = await l10n.formatValues([{id: "key1"}, {id: "key2"}]);

  strictEqual(values[0], "Value1");
  strictEqual(values[1], "Value2");

  values = await l10n.formatValues(["key1", {id: "key2"}]);

  strictEqual(values[0], "Value1");
  strictEqual(values[1], "Value2");

  values = await l10n.formatValues([{id: "key1"}, "key2"]);

  strictEqual(values[0], "Value1");
  strictEqual(values[1], "Value2");

  l10n.removeResourceIds(["/browser/menu.ftl"]);

  values = await l10n.formatValues([{id: "key1"}, {id: "key2"}]);

  strictEqual(values[0], null);
  strictEqual(values[1], "Value2");
});

add_task(async function test_switch_to_async() {
  const l10nReg = new L10nRegistry();

  const fs = [
    { path: "/localization/en-US/browser/menu.ftl", source: "key1 = Value1" },
    { path: "/localization/en-US/toolkit/menu.ftl", source: "key2 = Value2" },
  ];

  const source = L10nFileSource.createMock("test", "app", ["en-US"], "/localization/{locale}", fs);
  l10nReg.registerSources([source]);

  const l10n = new Localization(["/browser/menu.ftl"], true, l10nReg, ["en-US"]);

  let values = l10n.formatValuesSync([{id: "key1"}, {id: "key2"}]);

  strictEqual(values[0], "Value1");
  strictEqual(values[1], null);

  l10n.setAsync();

  Assert.throws(() => {
    l10n.formatValuesSync([{ id: "key1" }, { id: "key2" }]);
  }, /Can't use formatValuesSync when state is async./);

  l10n.addResourceIds(["/toolkit/menu.ftl"]);

  values = await l10n.formatValues([{id: "key1"}, {id: "key2"}]);
  let values2 = await l10n.formatValues([{id: "key1"}, {id: "key2"}]);

  deepEqual(values, values2);
  strictEqual(values[0], "Value1");
  strictEqual(values[1], "Value2");

  l10n.removeResourceIds(["/browser/menu.ftl"]);

  values = await l10n.formatValues([{id: "key1"}, {id: "key2"}]);

  strictEqual(values[0], null);
  strictEqual(values[1], "Value2");
});
