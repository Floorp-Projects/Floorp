/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function() {
  // So we have a browser session.
  for (let i = 1; i <= 30; i++) {
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "http://example.com/"
    );
    BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function test_sessions_get_recently_closed_max_results() {
  async function background() {
    let recentlyClosed = await browser.sessions.getRecentlyClosed();
    browser.test.assertEq(
      browser.sessions.MAX_SESSION_RESULTS,
      recentlyClosed.length,
      `The maximum number of sessions is ${browser.sessions.MAX_SESSION_RESULTS}`
    );
    browser.test.notifyPass("getRecentlyClosed with max results");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["sessions"],
    },
    background,
  });

  // Overwrite the "browser.sessionstore.max_tabs_undo" pref, because
  // without the fix the test would still pass (default on desktop=25).
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionstore.max_tabs_undo", 30]],
  });

  await extension.startup();
  await extension.awaitFinish("getRecentlyClosed with max results");
  await extension.unload();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_sessions_get_recently_closed_max_results_20() {
  async function background() {
    let recentlyClosed = await browser.sessions.getRecentlyClosed();
    browser.test.assertEq(
      20,
      recentlyClosed.length,
      `The maximum number of sessions is 20`
    );
    browser.test.notifyPass("getRecentlyClosed with max results 20");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["sessions"],
    },
    background,
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionstore.max_tabs_undo", 20]],
  });

  await extension.startup();
  await extension.awaitFinish("getRecentlyClosed with max results 20");
  await extension.unload();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_sessions_max_session_results_value() {
  function background() {
    browser.test.assertEq(
      25,
      browser.sessions.MAX_SESSION_RESULTS,
      `The value of sessions.MAX_SESSION_RESULTS is 25`
    );
    browser.test.notifyPass("max session results value");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["sessions"],
    },
    background,
  });

  await extension.startup();
  await extension.awaitFinish("max session results value");
  await extension.unload();
});
