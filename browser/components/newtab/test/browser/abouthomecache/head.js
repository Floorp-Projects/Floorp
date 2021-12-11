/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { AboutHomeStartupCache } = ChromeUtils.import(
  "resource:///modules/BrowserGlue.jsm"
);

// Some Activity Stream preferences are JSON encoded, and quite complex.
// Hard-coding them here or in browser.ini makes them brittle to change.
// Instead, we pull the default prefs structures and set the values that
// we need and write them to preferences here dynamically. We do this in
// its own scope to avoid polluting the global scope.
{
  const { PREFS_CONFIG } = ChromeUtils.import(
    "resource://activity-stream/lib/ActivityStream.jsm"
  );

  let defaultDSConfig = JSON.parse(
    PREFS_CONFIG.get("discoverystream.config").getValue({
      geo: "US",
      locale: "en-US",
    })
  );

  let newConfig = Object.assign(defaultDSConfig, {
    show_spocs: false,
    hardcoded_layout: false,
    layout_endpoint:
      "https://example.com/browser/browser/components/newtab/test/browser/ds_layout.json",
  });

  // Configure Activity Stream to query for the layout JSON file that points
  // at the local top stories feed.
  Services.prefs.setCharPref(
    "browser.newtabpage.activity-stream.discoverystream.config",
    JSON.stringify(newConfig)
  );
}

/**
 * Shuts down the AboutHomeStartupCache components in the parent process
 * and privileged about content process, and then restarts them, simulating
 * the parent process having restarted.
 *
 * @param browser (<xul:browser>)
 *   A <xul:browser> with about:home running in it. This will be reloaded
 *   after the restart simultion is complete, and that reload will attempt
 *   to read any about:home cache contents.
 * @param options (object, optional)
 *
 *   An object with the following properties:
 *
 *     withAutoShutdownWrite (boolean, optional):
 *       Whether or not the shutdown part of the simulation should cause the
 *       shutdown handler to run, which normally causes the cache to be
 *       written. Setting this to false is handy if the cache has been
 *       specially prepared for the subsequent startup, and we don't want to
 *       overwrite it. This defaults to true.
 *
 *     ensureCacheWinsRace (boolean, optional):
 *       Ensures that the privileged about content process will be able to
 *       read the bytes from the streams sent down from the HTTP cache. Use
 *       this to avoid the HTTP cache "losing the race" against reading the
 *       about:home document from the omni.ja. This defaults to true.
 *
 *     expectTimeout (boolean, optional):
 *       If true, indicates that it's expected that AboutHomeStartupCache will
 *       timeout when shutting down. If false, such timeouts will result in
 *       test failures. Defaults to false.
 *
 * @returns Promise
 * @resolves undefined
 *   Resolves once the restart simulation is complete, and the <xul:browser>
 *   pointed at about:home finishes reloading.
 */
// eslint-disable-next-line no-unused-vars
async function simulateRestart(
  browser,
  {
    withAutoShutdownWrite = true,
    ensureCacheWinsRace = true,
    expectTimeout = false,
  } = {}
) {
  info("Simulating restart of the browser");
  if (browser.remoteType !== E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE) {
    throw new Error(
      "prepareLoadFromCache should only be called on a browser " +
        "loaded in the privileged about content process."
    );
  }

  if (withAutoShutdownWrite && AboutHomeStartupCache.initted) {
    info("Simulating shutdown write");
    let timedOut = !(await AboutHomeStartupCache.onShutdown(expectTimeout));
    if (timedOut && !expectTimeout) {
      Assert.ok(
        false,
        "AboutHomeStartupCache shutdown unexpectedly timed out."
      );
    } else if (!timedOut && expectTimeout) {
      Assert.ok(false, "AboutHomeStartupCache shutdown failed to time out.");
    }
    info("Shutdown write done");
  } else {
    info("Intentionally skipping shutdown write");
  }

  AboutHomeStartupCache.uninit();

  info("Waiting for AboutHomeStartupCacheChild to uninit");
  await SpecialPowers.spawn(browser, [], async () => {
    let { AboutHomeStartupCacheChild } = ChromeUtils.import(
      "resource:///modules/AboutNewTabService.jsm"
    );
    AboutHomeStartupCacheChild.uninit();
  });
  info("AboutHomeStartupCacheChild uninitted");

  AboutHomeStartupCache.init();

  if (AboutHomeStartupCache.initted) {
    let processManager = browser.messageManager.processMessageManager;
    let pp = browser.browsingContext.currentWindowGlobal.domProcess;
    let { childID } = pp;
    AboutHomeStartupCache.onContentProcessCreated(childID, processManager, pp);

    info("Waiting for AboutHomeStartupCache cache entry");
    await AboutHomeStartupCache.ensureCacheEntry();
    info("Got AboutHomeStartupCache cache entry");

    if (ensureCacheWinsRace) {
      info("Ensuring cache bytes are available");
      await SpecialPowers.spawn(browser, [], async () => {
        let { AboutHomeStartupCacheChild } = ChromeUtils.import(
          "resource:///modules/AboutNewTabService.jsm"
        );
        let pageStream = AboutHomeStartupCacheChild._pageInputStream;
        let scriptStream = AboutHomeStartupCacheChild._scriptInputStream;
        await ContentTaskUtils.waitForCondition(() => {
          return pageStream.available() && scriptStream.available();
        });
      });
    }
  }

  info("Waiting for about:home to load");
  let loaded = BrowserTestUtils.browserLoaded(browser, false, "about:home");
  BrowserTestUtils.loadURI(browser, "about:home");
  await loaded;
  info("about:home loaded");
}

/**
 * Writes a page string and a script string into the cache for
 * the next about:home load.
 *
 * @param page (String)
 *   The HTML content to write into the cache. This cannot be the empty
 *   string. Note that this string should contain a node that has an
 *   id of "root", in order for the newtab scripts to attach correctly.
 *   Otherwise, an exception might get thrown which can cause shutdown
 *   leaks.
 * @param script (String)
 *   The JS content to write into the cache that can be loaded via
 *   about:home?jscache. This cannot be the empty string.
 * @returns Promise
 * @resolves undefined
 *   When the page and script content has been successfully written.
 */
// eslint-disable-next-line no-unused-vars
async function injectIntoCache(page, script) {
  if (!page || !script) {
    throw new Error("Cannot injectIntoCache with falsey values");
  }

  if (!page.includes(`id="root"`)) {
    throw new Error("Page markup must include a root node.");
  }

  await AboutHomeStartupCache.ensureCacheEntry();

  let pageInputStream = Cc[
    "@mozilla.org/io/string-input-stream;1"
  ].createInstance(Ci.nsIStringInputStream);

  pageInputStream.setUTF8Data(page);

  let scriptInputStream = Cc[
    "@mozilla.org/io/string-input-stream;1"
  ].createInstance(Ci.nsIStringInputStream);

  scriptInputStream.setUTF8Data(script);

  await AboutHomeStartupCache.populateCache(pageInputStream, scriptInputStream);
}

/**
 * Clears out any pre-existing about:home cache.
 * @returns Promise
 * @resolves undefined
 *   Resolves when the cache is cleared.
 */
// eslint-disable-next-line no-unused-vars
async function clearCache() {
  info("Test is clearing the cache");
  AboutHomeStartupCache.clearCache();
  await AboutHomeStartupCache.ensureCacheEntry();
  info("Test has cleared the cache.");
}

/**
 * Checks that the browser.startup.abouthome_cache_result scalar was
 * recorded at a particular value.
 *
 * @param cacheResultScalar (Number)
 *   One of the AboutHomeStartupCache.CACHE_RESULT_SCALARS values.
 */
function assertCacheResultScalar(cacheResultScalar) {
  let parentScalars = Services.telemetry.getSnapshotForScalars("main").parent;
  Assert.equal(
    parentScalars["browser.startup.abouthome_cache_result"],
    cacheResultScalar,
    "Expected the right value set to browser.startup.abouthome_cache_result " +
      "scalar."
  );
}

/**
 * Tests that the about:home document loaded in a passed <xul:browser> was
 * one from the cache.
 *
 * We test for this by looking for some tell-tale signs of the cached
 * document:
 *
 *   1. The about:home?jscache <script> element
 *   2. The __FROM_STARTUP_CACHE__ expando on the window
 *   3. The "activity-stream" class on the document body
 *   4. The top sites section
 *
 * @param browser (<xul:browser>)
 *   A <xul:browser> with about:home running in it.
 * @returns Promise
 * @resolves undefined
 *   Resolves once the cache entry has been destroyed.
 */
// eslint-disable-next-line no-unused-vars
async function ensureCachedAboutHome(browser) {
  await SpecialPowers.spawn(browser, [], async () => {
    let scripts = Array.from(content.document.querySelectorAll("script"));
    Assert.ok(!!scripts.length, "There should be page scripts.");
    let [lastScript] = scripts.reverse();
    Assert.equal(
      lastScript.src,
      "about:home?jscache",
      "Found about:home?jscache script tag, indicating the cached doc"
    );
    Assert.ok(
      Cu.waiveXrays(content).__FROM_STARTUP_CACHE__,
      "Should have found window.__FROM_STARTUP_CACHE__"
    );
    Assert.ok(
      content.document.body.classList.contains("activity-stream"),
      "Should have found activity-stream class on <body> element"
    );
    Assert.ok(
      content.document.querySelector("[data-section-id='topsites']"),
      "Should have found the Discovery Stream top sites."
    );
  });
  assertCacheResultScalar(
    AboutHomeStartupCache.CACHE_RESULT_SCALARS.VALID_AND_USED
  );
}

/**
 * Tests that the about:home document loaded in a passed <xul:browser> was
 * dynamically generated, and _not_ from the cache.
 *
 * We test for this by looking for some tell-tale signs of the dynamically
 * generated document:
 *
 *   1. No <script> elements (the scripts are loaded from the ScriptPreloader
 *      via AboutNewTabChild when the "privileged about content process" is
 *      enabled)
 *   2. No __FROM_STARTUP_CACHE__ expando on the window
 *   3. The "activity-stream" class on the document body
 *   4. The top sites section
 *
 * @param browser (<xul:browser>)
 *   A <xul:browser> with about:home running in it.
 * @param expectedResultScalar (Number)
 *   One of the AboutHomeStartupCache.CACHE_RESULT_SCALARS values. It is
 *   asserted that the cache result Telemetry scalar will have been set
 *   to this value to explain why the dynamic about:home was used.
 * @returns Promise
 * @resolves undefined
 *   Resolves once the cache entry has been destroyed.
 */
// eslint-disable-next-line no-unused-vars
async function ensureDynamicAboutHome(browser, expectedResultScalar) {
  await SpecialPowers.spawn(browser, [], async () => {
    let scripts = Array.from(content.document.querySelectorAll("script"));
    Assert.equal(scripts.length, 0, "There should be no page scripts.");

    Assert.equal(
      Cu.waiveXrays(content).__FROM_STARTUP_CACHE__,
      undefined,
      "Should not have found window.__FROM_STARTUP_CACHE__"
    );

    Assert.ok(
      content.document.body.classList.contains("activity-stream"),
      "Should have found activity-stream class on <body> element"
    );
    Assert.ok(
      content.document.querySelector("[data-section-id='topsites']"),
      "Should have found the Discovery Stream top sites."
    );
  });

  assertCacheResultScalar(expectedResultScalar);
}
