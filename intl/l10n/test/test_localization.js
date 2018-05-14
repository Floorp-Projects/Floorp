/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm", {});
const { Localization } = ChromeUtils.import("resource://gre/modules/Localization.jsm", {});
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});

add_task(function test_methods_presence() {
  equal(typeof Localization.prototype.formatValues, "function");
  equal(typeof Localization.prototype.formatMessages, "function");
  equal(typeof Localization.prototype.formatValue, "function");
});

add_task(async function test_methods_calling() {
  const { L10nRegistry, FileSource } =
    ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm", {});

  const fs = {
    "/localization/de/browser/menu.ftl": "key = [de] Value2",
    "/localization/en-US/browser/menu.ftl": "key = [en] Value2\nkey2 = [en] Value3",
  };
  const originalLoad = L10nRegistry.load;
  const originalRequested = Services.locale.getRequestedLocales();

  L10nRegistry.load = async function(url) {
    return fs[url];
  };

  const source = new FileSource("test", ["de", "en-US"], "/localization/{locale}");
  L10nRegistry.registerSource(source);

  async function* generateMessages(resIds) {
    yield * await L10nRegistry.generateContexts(["de", "en-US"], resIds);
  }

  const l10n = new Localization([
    "/browser/menu.ftl"
  ], generateMessages);

  let values = await l10n.formatValues([{id: "key"}, {id: "key2"}]);

  equal(values[0], "[de] Value2");
  equal(values[1], "[en] Value3");

  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
  L10nRegistry.load = originalLoad;
  Services.locale.setRequestedLocales(originalRequested);
});

add_task(async function test_builtins() {
  const { L10nRegistry, FileSource } =
    ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm", {});

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

  async function* generateMessages(resIds) {
    yield * await L10nRegistry.generateContexts(["en-US"], resIds);
  }

  const l10n = new Localization([
    "/test.ftl"
  ], generateMessages);

  let values = await l10n.formatValues([{id: "key"}]);

  ok(values[0].includes(
    `${ known_platforms[AppConstants.platform].toUpperCase() } Value`));

  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
  L10nRegistry.load = originalLoad;
});

add_task(async function test_add_remove_resourceIds() {
  const { L10nRegistry, FileSource } =
    ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm", {});

  const fs = {
    "/localization/en-US/browser/menu.ftl": "key1 = Value1",
    "/localization/en-US/toolkit/menu.ftl": "key2 = Value2",
  };
  const originalLoad = L10nRegistry.load;
  const originalRequested = Services.locale.getRequestedLocales();

  L10nRegistry.load = async function(url) {
    return fs[url];
  };

  const source = new FileSource("test", ["en-US"], "/localization/{locale}");
  L10nRegistry.registerSource(source);

  async function* generateMessages(resIds) {
    yield * await L10nRegistry.generateContexts(["en-US"], resIds);
  }

  const l10n = new Localization(["/browser/menu.ftl"], generateMessages);

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
  L10nRegistry.ctxCache.clear();
  L10nRegistry.load = originalLoad;
  Services.locale.setRequestedLocales(originalRequested);
});
