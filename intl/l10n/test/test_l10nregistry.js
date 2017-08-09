/* Any copyrighequal dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  L10nRegistry,
  FileSource,
  IndexedFileSource
} = Components.utils.import("resource://gre/modules/L10nRegistry.jsm", {});

let fs;
L10nRegistry.load = async function(url) {
  return fs[url];
}

add_task(function test_methods_presence() {
  equal(typeof L10nRegistry.generateContexts, "function");
  equal(typeof L10nRegistry.getAvailableLocales, "function");
  equal(typeof L10nRegistry.registerSource, "function");
  equal(typeof L10nRegistry.updateSource, "function");
});

/**
 * This test tests generation of a proper context for a single
 * source scenario
 */
add_task(async function test_methods_calling() {
  fs = {
    '/localization/en-US/browser/menu.ftl': 'key = Value',
  };
  const originalLoad = L10nRegistry.load;

  const source = new FileSource('test', ['en-US'], '/localization/{locale}');
  L10nRegistry.registerSource(source);

  const ctxs = L10nRegistry.generateContexts(['en-US'], ['/browser/menu.ftl']);

  const ctx = await ctxs.next().value;

  equal(ctx.hasMessage('key'), true);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test verifies that the public methods return expected values
 * for the single source scenario
 */
add_task(async function test_has_one_source() {
  let oneSource = new FileSource('app', ['en-US'], './app/data/locales/{locale}/');
  fs = {
    './app/data/locales/en-US/test.ftl': 'key = value en-US'
  };
  L10nRegistry.registerSource(oneSource);


  // has one source

  equal(L10nRegistry.sources.size, 1);
  equal(L10nRegistry.sources.has('app'), true);


  // returns a single context

  let ctxs = L10nRegistry.generateContexts(['en-US'], ['test.ftl']);
  let ctx0 = await ctxs.next().value;
  equal(ctx0.hasMessage('key'), true);

  equal(ctxs.next().done, true);


  // returns no contexts for missing locale

  ctxs = L10nRegistry.generateContexts(['pl'], ['test.ftl']);

  equal(ctxs.next().done, true);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test verifies that public methods return expected values
 * for the dual source scenario.
 */
add_task(async function test_has_two_sources() {
  let oneSource = new FileSource('platform', ['en-US'], './platform/data/locales/{locale}/');
  L10nRegistry.registerSource(oneSource);

  let secondSource = new FileSource('app', ['pl'], './app/data/locales/{locale}/');
  L10nRegistry.registerSource(secondSource);
  fs = {
    './platform/data/locales/en-US/test.ftl': 'key = platform value',
    './app/data/locales/pl/test.ftl': 'key = app value'
  };


  // has two sources

  equal(L10nRegistry.sources.size, 2);
  equal(L10nRegistry.sources.has('app'), true);
  equal(L10nRegistry.sources.has('platform'), true);


  // returns correct contexts for en-US

  let ctxs = L10nRegistry.generateContexts(['en-US'], ['test.ftl']);
  let ctx0 = await ctxs.next().value;

  equal(ctx0.hasMessage('key'), true);
  let msg = ctx0.getMessage('key');
  equal(ctx0.format(msg), 'platform value');

  equal(ctxs.next().done, true);


  // returns correct contexts for [pl, en-US]

  ctxs = L10nRegistry.generateContexts(['pl', 'en-US'], ['test.ftl']);
  ctx0 = await ctxs.next().value;
  equal(ctx0.locales[0], 'pl');
  equal(ctx0.hasMessage('key'), true);
  let msg0 = ctx0.getMessage('key');
  equal(ctx0.format(msg0), 'app value');

  let ctx1 = await ctxs.next().value;
  equal(ctx1.locales[0], 'en-US');
  equal(ctx1.hasMessage('key'), true);
  let msg1 = ctx1.getMessage('key');
  equal(ctx1.format(msg1), 'platform value');

  equal(ctxs.next().done, true);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test verifies that behavior specific to the IndexedFileSource
 * works correctly.
 *
 * In particular it tests that IndexedFileSource correctly returns
 * missing files as `false` instead of `undefined`.
 */
add_task(async function test_indexed() {
  let oneSource = new IndexedFileSource('langpack-pl', ['pl'], '/data/locales/{locale}/', [
    '/data/locales/pl/test.ftl',
  ]);
  L10nRegistry.registerSource(oneSource);
  fs = {
    '/data/locales/pl/test.ftl': 'key = value'
  };

  equal(L10nRegistry.sources.size, 1);
  equal(L10nRegistry.sources.has('langpack-pl'), true);

  equal(oneSource.getPath('pl', 'test.ftl'), '/data/locales/pl/test.ftl');
  equal(oneSource.hasFile('pl', 'test.ftl'), true);
  equal(oneSource.hasFile('pl', 'missing.ftl'), false);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test checks if the correct order of contexts is used for
 * scenarios where a new file source is added on top of the default one.
 */
add_task(async function test_override() {
  let fileSource = new FileSource('app', ['pl'], '/app/data/locales/{locale}/');
  L10nRegistry.registerSource(fileSource);

  let oneSource = new IndexedFileSource('langpack-pl', ['pl'], '/data/locales/{locale}/', [
    '/data/locales/pl/test.ftl'
  ]);
  L10nRegistry.registerSource(oneSource);

  fs = {
    '/app/data/locales/pl/test.ftl': 'key = value',
    '/data/locales/pl/test.ftl': 'key = addon value'
  };

  equal(L10nRegistry.sources.size, 2);
  equal(L10nRegistry.sources.has('langpack-pl'), true);

  let ctxs = L10nRegistry.generateContexts(['pl'], ['test.ftl']);
  let ctx0 = await ctxs.next().value;
  equal(ctx0.locales[0], 'pl');
  equal(ctx0.hasMessage('key'), true);
  let msg0 = ctx0.getMessage('key');
  equal(ctx0.format(msg0), 'addon value');

  let ctx1 = await ctxs.next().value;
  equal(ctx1.locales[0], 'pl');
  equal(ctx1.hasMessage('key'), true);
  let msg1 = ctx1.getMessage('key');
  equal(ctx1.format(msg1), 'value');

  equal(ctxs.next().done, true);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test verifies that new contexts are returned
 * after source update.
 */
add_task(async function test_updating() {
  let oneSource = new IndexedFileSource('langpack-pl', ['pl'], '/data/locales/{locale}/', [
    '/data/locales/pl/test.ftl',
  ]);
  L10nRegistry.registerSource(oneSource);
  fs = {
    '/data/locales/pl/test.ftl': 'key = value'
  };

  let ctxs = L10nRegistry.generateContexts(['pl'], ['test.ftl']);
  let ctx0 = await ctxs.next().value;
  equal(ctx0.locales[0], 'pl');
  equal(ctx0.hasMessage('key'), true);
  let msg0 = ctx0.getMessage('key');
  equal(ctx0.format(msg0), 'value');


  const newSource = new IndexedFileSource('langpack-pl', ['pl'], '/data/locales/{locale}/', [
    '/data/locales/pl/test.ftl'
  ]);
  fs['/data/locales/pl/test.ftl'] = 'key = new value';
  L10nRegistry.updateSource(newSource);

  equal(L10nRegistry.sources.size, 1);
  ctxs = L10nRegistry.generateContexts(['pl'], ['test.ftl']);
  ctx0 = await ctxs.next().value;
  msg0 = ctx0.getMessage('key');
  equal(ctx0.format(msg0), 'new value');

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test verifies that generated contexts return correct values
 * after sources are being removed.
 */
add_task(async function test_removing() {
  let fileSource = new FileSource('app', ['pl'], '/app/data/locales/{locale}/');
  L10nRegistry.registerSource(fileSource);

  let oneSource = new IndexedFileSource('langpack-pl', ['pl'], '/data/locales/{locale}/', [
    '/data/locales/pl/test.ftl'
  ]);
  L10nRegistry.registerSource(oneSource);

  fs = {
    '/app/data/locales/pl/test.ftl': 'key = value',
    '/data/locales/pl/test.ftl': 'key = addon value'
  };

  equal(L10nRegistry.sources.size, 2);
  equal(L10nRegistry.sources.has('langpack-pl'), true);

  let ctxs = L10nRegistry.generateContexts(['pl'], ['test.ftl']);
  let ctx0 = await ctxs.next().value;
  equal(ctx0.locales[0], 'pl');
  equal(ctx0.hasMessage('key'), true);
  let msg0 = ctx0.getMessage('key');
  equal(ctx0.format(msg0), 'addon value');

  let ctx1 = await ctxs.next().value;
  equal(ctx1.locales[0], 'pl');
  equal(ctx1.hasMessage('key'), true);
  let msg1 = ctx1.getMessage('key');
  equal(ctx1.format(msg1), 'value');

  equal(ctxs.next().done, true);

  // Remove langpack

  L10nRegistry.removeSource('langpack-pl');

  equal(L10nRegistry.sources.size, 1);
  equal(L10nRegistry.sources.has('langpack-pl'), false);

  ctxs = L10nRegistry.generateContexts(['pl'], ['test.ftl']);
  ctx0 = await ctxs.next().value;
  equal(ctx0.locales[0], 'pl');
  equal(ctx0.hasMessage('key'), true);
  msg0 = ctx0.getMessage('key');
  equal(ctx0.format(msg0), 'value');

  equal(ctxs.next().done, true);

  // Remove app source

  L10nRegistry.removeSource('app');

  equal(L10nRegistry.sources.size, 0);

  ctxs = L10nRegistry.generateContexts(['pl'], ['test.ftl']);
  equal(ctxs.next().done, true);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});
