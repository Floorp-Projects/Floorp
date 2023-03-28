/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

/**
 * Tests that if the browser is restoring the last session, not because
 * it has been configured to, but because of a special condition (recovery
 * or after applying an update), that we ignore the about:home startup
 * cache, since it's unlikely to be the first thing loaded.
 */
add_task(async function test_no_cache_on_SessionStartup_restore() {
  let sandbox = sinon.createSandbox();

  // Let's fake out the condition where SessionStartup thinks it's going to
  // provide a restored session on startup.
  sandbox.stub(SessionStartup, "willRestore").callsFake(() => {
    return true;
  });

  await withFullyLoadedAboutHome(async browser => {
    await simulateRestart(browser);

    await ensureDynamicAboutHome(
      browser,
      AboutHomeStartupCache.CACHE_RESULT_SCALARS.NOT_LOADING_ABOUTHOME
    );
  });

  sandbox.restore();
});
