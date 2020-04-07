/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

add_task(function test_methods_presence() {
  equal(typeof Localization.prototype.formatValues, "function");
  equal(typeof Localization.prototype.formatMessages, "function");
  equal(typeof Localization.prototype.formatValue, "function");
});

add_task(async function test_methods_calling() {
  const { L10nRegistry, FileSource } =
    ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm");

  const fs = {
    "/localization/de/browser/menu.ftl": "key = [de] Value2",
    "/localization/en-US/browser/menu.ftl": "key = [en] Value2\nkey2 = [en] Value3",
  };
  const originalLoad = L10nRegistry.load;
  const originalRequested = Services.locale.requestedLocales;

  L10nRegistry.load = async function(url) {
    return fs[url];
  };

  const source = new FileSource("test", ["de", "en-US"], "/localization/{locale}");
  L10nRegistry.registerSource(source);

  async function* generateBundles(resIds) {
    yield * await L10nRegistry.generateBundles(["de", "en-US"], resIds);
  }

  const l10n = new Localization([
    "/browser/menu.ftl",
  ], false, { generateBundles });

  let values = await l10n.formatValues([{id: "key"}, {id: "key2"}]);

  equal(values[0], "[de] Value2");
  equal(values[1], "[en] Value3");

  L10nRegistry.sources.clear();
  L10nRegistry.load = originalLoad;
  Services.locale.requestedLocales = originalRequested;
});

add_task(async function test_builtins() {
  const { L10nRegistry, FileSource } =
    ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm");

  const known_platforms = {
    "linux": "linux",
    "win": "windows",
    "macosx": "macos",
    "android": "android",
  };

  const fs = {
    "/localization/en-US/test.ftl": `
key = { PLATFORM() ->
        ${ Object.values(known_platforms).map(
              name => `      [${ name }] ${ name.toUpperCase() } Value\n`).join("") }
       *[other] OTHER Value
    }`,
  };
  const originalLoad = L10nRegistry.load;

  L10nRegistry.load = async function(url) {
    return fs[url];
  };

  const source = new FileSource("test", ["en-US"], "/localization/{locale}");
  L10nRegistry.registerSource(source);

  async function* generateBundles(resIds) {
    yield * await L10nRegistry.generateBundles(["en-US"], resIds);
  }

  const l10n = new Localization([
    "/test.ftl",
  ], false, { generateBundles });

  let values = await l10n.formatValues([{id: "key"}]);

  ok(values[0].includes(
    `${ known_platforms[AppConstants.platform].toUpperCase() } Value`));

  L10nRegistry.sources.clear();
  L10nRegistry.load = originalLoad;
});

add_task(async function test_add_remove_resourceIds() {
  const { L10nRegistry, FileSource } =
    ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm");

  const fs = {
    "/localization/en-US/browser/menu.ftl": "key1 = Value1",
    "/localization/en-US/toolkit/menu.ftl": "key2 = Value2",
  };
  const originalLoad = L10nRegistry.load;
  const originalRequested = Services.locale.requestedLocales;

  L10nRegistry.load = async function(url) {
    return fs[url];
  };

  const source = new FileSource("test", ["en-US"], "/localization/{locale}");
  L10nRegistry.registerSource(source);

  async function* generateBundles(resIds) {
    yield * await L10nRegistry.generateBundles(["en-US"], resIds);
  }

  const l10n = new Localization(["/browser/menu.ftl"], false, { generateBundles });

  let values = await l10n.formatValues([{id: "key1"}, {id: "key2"}]);

  equal(values[0], "Value1");
  equal(values[1], undefined);

  l10n.addResourceIds(["/toolkit/menu.ftl"]);

  values = await l10n.formatValues([{id: "key1"}, {id: "key2"}]);

  equal(values[0], "Value1");
  equal(values[1], "Value2");

  l10n.removeResourceIds(["/browser/menu.ftl"]);

  values = await l10n.formatValues([{id: "key1"}, {id: "key2"}]);

  equal(values[0], undefined);
  equal(values[1], "Value2");

  L10nRegistry.sources.clear();
  L10nRegistry.load = originalLoad;
  Services.locale.requestedLocales = originalRequested;
});

add_task(async function test_switch_to_async() {
  const { L10nRegistry, FileSource } =
    ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm");

  const fs = {
    "/localization/en-US/browser/menu.ftl": "key1 = Value1",
    "/localization/en-US/toolkit/menu.ftl": "key2 = Value2",
  };
  const originalLoad = L10nRegistry.load;
  const originalLoadSync = L10nRegistry.loadSync;
  const originalRequested = Services.locale.requestedLocales;

  let syncLoads = 0;
  let asyncLoads = 0;

  L10nRegistry.load = async function(url) {
    asyncLoads += 1;
    return fs[url];
  };

  L10nRegistry.loadSync = function(url) {
    syncLoads += 1;
    return fs[url];
  };

  const source = new FileSource("test", ["en-US"], "/localization/{locale}");
  L10nRegistry.registerSource(source);

  async function* generateBundles(resIds) {
    yield * await L10nRegistry.generateBundles(["en-US"], resIds);
  }

  function* generateBundlesSync(resIds) {
    yield * L10nRegistry.generateBundlesSync(["en-US"], resIds);
  }

  const l10n = new Localization(["/browser/menu.ftl"], false, { generateBundles, generateBundlesSync });

  let values = await l10n.formatValues([{id: "key1"}, {id: "key2"}]);

  equal(values[0], "Value1");
  equal(values[1], undefined);
  equal(syncLoads, 0);
  equal(asyncLoads, 1);

  l10n.setIsSync(true);

  l10n.addResourceIds(["/toolkit/menu.ftl"]);

  // Nothing happens when we switch, because
  // the next load is lazy.
  equal(syncLoads, 0);
  equal(asyncLoads, 1);

  values = l10n.formatValuesSync([{id: "key1"}, {id: "key2"}]);
  let values2 = await l10n.formatValues([{id: "key1"}, {id: "key2"}]);

  deepEqual(values, values2);
  equal(values[0], "Value1");
  equal(values[1], "Value2");
  equal(syncLoads, 1);
  equal(asyncLoads, 1);

  l10n.removeResourceIds(["/browser/menu.ftl"]);

  values = await l10n.formatValues([{id: "key1"}, {id: "key2"}]);

  equal(values[0], undefined);
  equal(values[1], "Value2");
  equal(syncLoads, 1);
  equal(asyncLoads, 1);

  L10nRegistry.sources.clear();
  L10nRegistry.load = originalLoad;
  L10nRegistry.loadSync = originalLoadSync;
  Services.locale.requestedLocales = originalRequested;
});
