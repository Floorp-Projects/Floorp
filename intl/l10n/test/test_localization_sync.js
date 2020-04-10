/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

add_task(function test_methods_calling() {
  const { L10nRegistry, FileSource } =
    ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm");

  const fs = {
    "/localization/de/browser/menu.ftl": "key = [de] Value2",
    "/localization/en-US/browser/menu.ftl": "key = [en] Value2\nkey2 = [en] Value3",
  };
  const originalLoadSync = L10nRegistry.loadSync;
  const originalRequested = Services.locale.requestedLocales;

  L10nRegistry.loadSync = function(url) {
    return fs[url];
  };

  const source = new FileSource("test", ["de", "en-US"], "/localization/{locale}");
  L10nRegistry.registerSource(source);

  function* generateBundlesSync(resIds) {
    yield * L10nRegistry.generateBundlesSync(["de", "en-US"], resIds);
  }

  const l10n = new Localization([
    "/browser/menu.ftl",
  ], true, { generateBundlesSync });

  let values = l10n.formatValuesSync([{id: "key"}, {id: "key2"}]);

  equal(values[0], "[de] Value2");
  equal(values[1], "[en] Value3");

  L10nRegistry.sources.clear();
  L10nRegistry.loadSync = originalLoadSync;
  Services.locale.requestedLocales = originalRequested;
});

add_task(function test_builtins() {
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
  const originalLoadSync = L10nRegistry.loadSync;

  L10nRegistry.loadSync = function(url) {
    return fs[url];
  };

  const source = new FileSource("test", ["en-US"], "/localization/{locale}");
  L10nRegistry.registerSource(source);

  function* generateBundlesSync(resIds) {
    yield * L10nRegistry.generateBundlesSync(["en-US"], resIds);
  }

  const l10n = new Localization([
    "/test.ftl",
  ], true, { generateBundlesSync });

  let values = l10n.formatValuesSync([{id: "key"}]);

  ok(values[0].includes(
    `${ known_platforms[AppConstants.platform].toUpperCase() } Value`));

  L10nRegistry.sources.clear();
  L10nRegistry.loadSync = originalLoadSync;
});

add_task(function test_add_remove_resourceIds() {
  const { L10nRegistry, FileSource } =
    ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm");

  const fs = {
    "/localization/en-US/browser/menu.ftl": "key1 = Value1",
    "/localization/en-US/toolkit/menu.ftl": "key2 = Value2",
  };
  const originalLoadSync = L10nRegistry.loadSYnc;
  const originalRequested = Services.locale.requestedLocales;

  L10nRegistry.loadSync = function(url) {
    return fs[url];
  };

  const source = new FileSource("test", ["en-US"], "/localization/{locale}");
  L10nRegistry.registerSource(source);

  function* generateBundlesSync(resIds) {
    yield * L10nRegistry.generateBundlesSync(["en-US"], resIds);
  }

  const l10n = new Localization(["/browser/menu.ftl"], true, { generateBundlesSync });

  let values = l10n.formatValuesSync([{id: "key1"}, {id: "key2"}]);

  equal(values[0], "Value1");
  equal(values[1], undefined);

  l10n.addResourceIds(["/toolkit/menu.ftl"]);

  values = l10n.formatValuesSync([{id: "key1"}, {id: "key2"}]);

  equal(values[0], "Value1");
  equal(values[1], "Value2");

  l10n.removeResourceIds(["/browser/menu.ftl"]);

  values = l10n.formatValuesSync([{id: "key1"}, {id: "key2"}]);

  equal(values[0], undefined);
  equal(values[1], "Value2");

  L10nRegistry.sources.clear();
  L10nRegistry.loadSync = originalLoadSync;
  Services.locale.requestedLocales = originalRequested;
});

add_task(function test_calling_sync_methods_in_async_mode_fails() {
  const l10n = new Localization(["/browser/menu.ftl"], false);

  Assert.throws(() => {
    l10n.formatValuesSync([{ id: "key1" }, { id: "key2" }]);
  }, /Can't use sync formatWithFallback when state is async./);

  Assert.throws(() => {
    l10n.formatValueSync("key1");
  }, /Can't use sync formatWithFallback when state is async./);

  Assert.throws(() => {
    l10n.formatMessagesSync([{ id: "key1"}]);
  }, /Can't use sync formatWithFallback when state is async./);
});
