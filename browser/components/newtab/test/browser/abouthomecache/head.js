/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { AboutHomeStartupCache } = ChromeUtils.import(
  "resource:///modules/BrowserGlue.jsm"
);

/**
 * Shuts down the AboutHomeStartupCache components in the parent process
 * and privileged about content process, and then restarts them, simulating
 * the parent process having restarted.
 *
 * @param browser (<xul:browser>)
 *   A <xul:browser> with about:home running in it. This will be reloaded
 *   after the restart simultion is complete, and that reload will attempt
 *   to read any about:home cache contents.
 * @param withAutoShutdownWrite (Boolean, optional)
 *   Whether or not the shutdown part of the simulation should cause the
 *   shutdown handler to run, which normally causes the cache to be
 *   written. Setting this to false is handy if the cache has been specially
 *   prepared for the subsequent startup, and we don't want to overwrite
 *   it.
 * @returns Promise
 * @resolves undefined
 *   Resolves once the restart simulation is complete, and the <xul:browser>
 *   pointed at about:home finishes reloading.
 */
// eslint-disable-next-line no-unused-vars
async function simulateRestart(browser, withAutoShutdownWrite = true) {
  let processManager = browser.messageManager.processMessageManager;
  if (processManager.remoteType !== E10SUtils.PRIVILEGEDABOUT_REMOTE_TYPE) {
    throw new Error(
      "prepareLoadFromCache should only be called on a browser " +
        "loaded in the privileged about content process."
    );
  }

  if (withAutoShutdownWrite) {
    await AboutHomeStartupCache.onShutdown();
  }

  AboutHomeStartupCache.uninit();

  await SpecialPowers.spawn(browser, [], async () => {
    let { AboutHomeStartupCacheChild } = ChromeUtils.import(
      "resource:///modules/AboutNewTabService.jsm"
    );
    AboutHomeStartupCacheChild.uninit();
  });

  AboutHomeStartupCache.init();

  AboutHomeStartupCache.sendCacheInputStreams(processManager);

  await AboutHomeStartupCache.ensureCacheEntry();

  let loaded = BrowserTestUtils.browserLoaded(browser, false, "about:home");
  browser.reload();
  await loaded;
}

/**
 * Writes a page string and a script string into the cache for
 * the next about:home load.
 *
 * @param page (String)
 *   The HTML content to write into the cache. This cannot be the empty
 *   string.
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
 */
// eslint-disable-next-line no-unused-vars
function clearCache() {
  AboutHomeStartupCache.clearCache();
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
 * @returns Promise
 * @resolves undefined
 *   Resolves once the cache entry has been destroyed.
 */
// eslint-disable-next-line no-unused-vars
async function ensureDynamicAboutHome(browser) {
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
}
