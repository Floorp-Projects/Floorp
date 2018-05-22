/* Any copyrighequal dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  L10nRegistry,
  FileSource,
  IndexedFileSource
} = ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm", {});
ChromeUtils.import("resource://gre/modules/Timer.jsm");

let fs;
L10nRegistry.load = async function(url) {
  if (!fs.hasOwnProperty(url)) {
    return Promise.reject("Resource unavailable");
  }
  return fs[url];
};

add_task(function test_methods_presence() {
  equal(typeof L10nRegistry.generateContexts, "function");
  equal(typeof L10nRegistry.getAvailableLocales, "function");
  equal(typeof L10nRegistry.registerSource, "function");
  equal(typeof L10nRegistry.updateSource, "function");
});

/**
 * Test that passing empty resourceIds list works.
 */
add_task(async function test_empty_resourceids() {
  fs = {};

  const source = new FileSource("test", ["en-US"], "/localization/{locale}");
  L10nRegistry.registerSource(source);

  const ctxs = L10nRegistry.generateContexts(["en-US"], []);

  const done = (await ctxs.next()).done;

  equal(done, true);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * Test that passing empty sources list works.
 */
add_task(async function test_empty_sources() {
  fs = {};

  const ctxs = L10nRegistry.generateContexts(["en-US"], []);

  const done = (await ctxs.next()).done;

  equal(done, true);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test tests generation of a proper context for a single
 * source scenario
 */
add_task(async function test_methods_calling() {
  fs = {
    "/localization/en-US/browser/menu.ftl": "key = Value",
  };

  const source = new FileSource("test", ["en-US"], "/localization/{locale}");
  L10nRegistry.registerSource(source);

  const ctxs = L10nRegistry.generateContexts(["en-US"], ["/browser/menu.ftl"]);

  const ctx = (await ctxs.next()).value;

  equal(ctx.hasMessage("key"), true);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test verifies that the public methods return expected values
 * for the single source scenario
 */
add_task(async function test_has_one_source() {
  let oneSource = new FileSource("app", ["en-US"], "./app/data/locales/{locale}/");
  fs = {
    "./app/data/locales/en-US/test.ftl": "key = value en-US"
  };
  L10nRegistry.registerSource(oneSource);


  // has one source

  equal(L10nRegistry.sources.size, 1);
  equal(L10nRegistry.sources.has("app"), true);


  // returns a single context

  let ctxs = L10nRegistry.generateContexts(["en-US"], ["test.ftl"]);
  let ctx0 = (await ctxs.next()).value;
  equal(ctx0.hasMessage("key"), true);

  equal((await ctxs.next()).done, true);


  // returns no contexts for missing locale

  ctxs = L10nRegistry.generateContexts(["pl"], ["test.ftl"]);

  equal((await ctxs.next()).done, true);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test verifies that public methods return expected values
 * for the dual source scenario.
 */
add_task(async function test_has_two_sources() {
  let oneSource = new FileSource("platform", ["en-US"], "./platform/data/locales/{locale}/");
  L10nRegistry.registerSource(oneSource);

  let secondSource = new FileSource("app", ["pl"], "./app/data/locales/{locale}/");
  L10nRegistry.registerSource(secondSource);
  fs = {
    "./platform/data/locales/en-US/test.ftl": "key = platform value",
    "./app/data/locales/pl/test.ftl": "key = app value"
  };


  // has two sources

  equal(L10nRegistry.sources.size, 2);
  equal(L10nRegistry.sources.has("app"), true);
  equal(L10nRegistry.sources.has("platform"), true);


  // returns correct contexts for en-US

  let ctxs = L10nRegistry.generateContexts(["en-US"], ["test.ftl"]);
  let ctx0 = (await ctxs.next()).value;

  equal(ctx0.hasMessage("key"), true);
  let msg = ctx0.getMessage("key");
  equal(ctx0.format(msg), "platform value");

  equal((await ctxs.next()).done, true);


  // returns correct contexts for [pl, en-US]

  ctxs = L10nRegistry.generateContexts(["pl", "en-US"], ["test.ftl"]);
  ctx0 = (await ctxs.next()).value;
  equal(ctx0.locales[0], "pl");
  equal(ctx0.hasMessage("key"), true);
  let msg0 = ctx0.getMessage("key");
  equal(ctx0.format(msg0), "app value");

  let ctx1 = (await ctxs.next()).value;
  equal(ctx1.locales[0], "en-US");
  equal(ctx1.hasMessage("key"), true);
  let msg1 = ctx1.getMessage("key");
  equal(ctx1.format(msg1), "platform value");

  equal((await ctxs.next()).done, true);

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
  let oneSource = new IndexedFileSource("langpack-pl", ["pl"], "/data/locales/{locale}/", [
    "/data/locales/pl/test.ftl",
  ]);
  L10nRegistry.registerSource(oneSource);
  fs = {
    "/data/locales/pl/test.ftl": "key = value"
  };

  equal(L10nRegistry.sources.size, 1);
  equal(L10nRegistry.sources.has("langpack-pl"), true);

  equal(oneSource.getPath("pl", "test.ftl"), "/data/locales/pl/test.ftl");
  equal(oneSource.hasFile("pl", "test.ftl"), true);
  equal(oneSource.hasFile("pl", "missing.ftl"), false);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test checks if the correct order of contexts is used for
 * scenarios where a new file source is added on top of the default one.
 */
add_task(async function test_override() {
  let fileSource = new FileSource("app", ["pl"], "/app/data/locales/{locale}/");
  L10nRegistry.registerSource(fileSource);

  let oneSource = new IndexedFileSource("langpack-pl", ["pl"], "/data/locales/{locale}/", [
    "/data/locales/pl/test.ftl"
  ]);
  L10nRegistry.registerSource(oneSource);

  fs = {
    "/app/data/locales/pl/test.ftl": "key = value",
    "/data/locales/pl/test.ftl": "key = addon value"
  };

  equal(L10nRegistry.sources.size, 2);
  equal(L10nRegistry.sources.has("langpack-pl"), true);

  let ctxs = L10nRegistry.generateContexts(["pl"], ["test.ftl"]);
  let ctx0 = (await ctxs.next()).value;
  equal(ctx0.locales[0], "pl");
  equal(ctx0.hasMessage("key"), true);
  let msg0 = ctx0.getMessage("key");
  equal(ctx0.format(msg0), "addon value");

  let ctx1 = (await ctxs.next()).value;
  equal(ctx1.locales[0], "pl");
  equal(ctx1.hasMessage("key"), true);
  let msg1 = ctx1.getMessage("key");
  equal(ctx1.format(msg1), "value");

  equal((await ctxs.next()).done, true);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test verifies that new contexts are returned
 * after source update.
 */
add_task(async function test_updating() {
  let oneSource = new IndexedFileSource("langpack-pl", ["pl"], "/data/locales/{locale}/", [
    "/data/locales/pl/test.ftl",
  ]);
  L10nRegistry.registerSource(oneSource);
  fs = {
    "/data/locales/pl/test.ftl": "key = value"
  };

  let ctxs = L10nRegistry.generateContexts(["pl"], ["test.ftl"]);
  let ctx0 = (await ctxs.next()).value;
  equal(ctx0.locales[0], "pl");
  equal(ctx0.hasMessage("key"), true);
  let msg0 = ctx0.getMessage("key");
  equal(ctx0.format(msg0), "value");


  const newSource = new IndexedFileSource("langpack-pl", ["pl"], "/data/locales/{locale}/", [
    "/data/locales/pl/test.ftl"
  ]);
  fs["/data/locales/pl/test.ftl"] = "key = new value";
  L10nRegistry.updateSource(newSource);

  equal(L10nRegistry.sources.size, 1);
  ctxs = L10nRegistry.generateContexts(["pl"], ["test.ftl"]);
  ctx0 = (await ctxs.next()).value;
  msg0 = ctx0.getMessage("key");
  equal(ctx0.format(msg0), "new value");

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test verifies that generated contexts return correct values
 * after sources are being removed.
 */
add_task(async function test_removing() {
  let fileSource = new FileSource("app", ["pl"], "/app/data/locales/{locale}/");
  L10nRegistry.registerSource(fileSource);

  let oneSource = new IndexedFileSource("langpack-pl", ["pl"], "/data/locales/{locale}/", [
    "/data/locales/pl/test.ftl"
  ]);
  L10nRegistry.registerSource(oneSource);

  fs = {
    "/app/data/locales/pl/test.ftl": "key = value",
    "/data/locales/pl/test.ftl": "key = addon value"
  };

  equal(L10nRegistry.sources.size, 2);
  equal(L10nRegistry.sources.has("langpack-pl"), true);

  let ctxs = L10nRegistry.generateContexts(["pl"], ["test.ftl"]);
  let ctx0 = (await ctxs.next()).value;
  equal(ctx0.locales[0], "pl");
  equal(ctx0.hasMessage("key"), true);
  let msg0 = ctx0.getMessage("key");
  equal(ctx0.format(msg0), "addon value");

  let ctx1 = (await ctxs.next()).value;
  equal(ctx1.locales[0], "pl");
  equal(ctx1.hasMessage("key"), true);
  let msg1 = ctx1.getMessage("key");
  equal(ctx1.format(msg1), "value");

  equal((await ctxs.next()).done, true);

  // Remove langpack

  L10nRegistry.removeSource("langpack-pl");

  equal(L10nRegistry.sources.size, 1);
  equal(L10nRegistry.sources.has("langpack-pl"), false);

  ctxs = L10nRegistry.generateContexts(["pl"], ["test.ftl"]);
  ctx0 = (await ctxs.next()).value;
  equal(ctx0.locales[0], "pl");
  equal(ctx0.hasMessage("key"), true);
  msg0 = ctx0.getMessage("key");
  equal(ctx0.format(msg0), "value");

  equal((await ctxs.next()).done, true);

  // Remove app source

  L10nRegistry.removeSource("app");

  equal(L10nRegistry.sources.size, 0);

  ctxs = L10nRegistry.generateContexts(["pl"], ["test.ftl"]);
  equal((await ctxs.next()).done, true);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test verifies that the logic works correctly when there's a missing
 * file in the FileSource scenario.
 */
add_task(async function test_missing_file() {
  let oneSource = new FileSource("app", ["en-US"], "./app/data/locales/{locale}/");
  L10nRegistry.registerSource(oneSource);
  let twoSource = new FileSource("platform", ["en-US"], "./platform/data/locales/{locale}/");
  L10nRegistry.registerSource(twoSource);

  fs = {
    "./app/data/locales/en-US/test.ftl": "key = value en-US",
    "./platform/data/locales/en-US/test.ftl": "key = value en-US",
    "./platform/data/locales/en-US/test2.ftl": "key2 = value2 en-US"
  };


  // has two sources

  equal(L10nRegistry.sources.size, 2);
  equal(L10nRegistry.sources.has("app"), true);
  equal(L10nRegistry.sources.has("platform"), true);


  // returns a single context

  let ctxs = L10nRegistry.generateContexts(["en-US"], ["test.ftl", "test2.ftl"]);
  (await ctxs.next());
  (await ctxs.next());

  equal((await ctxs.next()).done, true);


  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
});

/**
 * This test verifies that each file is that all files requested
 * by a single context are fetched at the same time, even
 * if one I/O is slow.
 */
add_task(async function test_parallel_io() {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  let originalLoad = L10nRegistry.load;
  let fetchIndex = new Map();

  L10nRegistry.load = function(url) {
    if (!fetchIndex.has(url)) {
      fetchIndex.set(url, 0);
    }
    fetchIndex.set(url, fetchIndex.get(url) + 1);

    if (url === "/en-US/slow-file.ftl") {
      return new Promise((resolve, reject) => {
        setTimeout(() => {
          // Despite slow-file being the first on the list,
          // by the time the it finishes loading, the other
          // two files are already fetched.
          equal(fetchIndex.get("/en-US/test.ftl"), 1);
          equal(fetchIndex.get("/en-US/test2.ftl"), 1);

          resolve("");
        }, 10);
      });
    }
    return Promise.resolve("");
  };
  let oneSource = new FileSource("app", ["en-US"], "/{locale}/");
  L10nRegistry.registerSource(oneSource);

  fs = {
    "/en-US/test.ftl": "key = value en-US",
    "/en-US/test2.ftl": "key2 = value2 en-US",
    "/en-US/slow-file.ftl": "key-slow = value slow en-US",
  };

  // returns a single context

  let ctxs = L10nRegistry.generateContexts(["en-US"], ["slow-file.ftl", "test.ftl", "test2.ftl"]);

  equal(fetchIndex.size, 0);

  let ctx0 = await ctxs.next();

  equal(ctx0.done, false);

  equal((await ctxs.next()).done, true);

  // When requested again, the cache should make the load operation not
  // increase the fetchedIndex count
  L10nRegistry.generateContexts(["en-US"], ["test.ftl", "test2.ftl", "slow-file.ftl"]);

  // cleanup
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
  L10nRegistry.load = originalLoad;
});
