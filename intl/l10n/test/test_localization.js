/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { Localization } = Components.utils.import("resource://gre/modules/Localization.jsm", {});

add_task(function test_methods_presence() {
  equal(typeof Localization.prototype.formatValues, "function");
  equal(typeof Localization.prototype.formatMessages, "function");
  equal(typeof Localization.prototype.formatValue, "function");
});

add_task(async function test_methods_calling() {
  const { L10nRegistry, FileSource } =
    Components.utils.import("resource://gre/modules/L10nRegistry.jsm", {});
  const LocaleService =
    Components.classes["@mozilla.org/intl/localeservice;1"].getService(
      Components.interfaces.mozILocaleService);

  const fs = {
    '/localization/de/browser/menu.ftl': 'key = [de] Value2',
    '/localization/en-US/browser/menu.ftl': 'key = [en] Value2\nkey2 = [en] Value3',
  };
  const originalLoad = L10nRegistry.load;
  const originalRequested = LocaleService.getRequestedLocales();

  L10nRegistry.load = function(url) {
    return fs[url];
  }

  const source = new FileSource('test', ['de', 'en-US'], '/localization/{locale}');
  L10nRegistry.registerSource(source);

  function * generateMessages(resIds) {
    yield * L10nRegistry.generateContexts(['de', 'en-US'], resIds);
  }

  const l10n = new Localization([
    '/browser/menu.ftl'
  ], generateMessages);

  let values = await l10n.formatValues([['key'], ['key2']]);

  equal(values[0], '[de] Value2');
  equal(values[1], '[en] Value3');

  L10nRegistry.sources.clear();
  L10nRegistry.load = originalLoad;
  LocaleService.setRequestedLocales(originalRequested);
});

