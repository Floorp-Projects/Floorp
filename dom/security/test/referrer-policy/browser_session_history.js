/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_FILE =
  "https://example.com/browser/dom/security/test/referrer-policy/file_session_history.sjs";

add_task(async function test_browser_navigation() {
  await BrowserTestUtils.withNewTab(TEST_FILE, async browser => {
    let loadPromise = BrowserTestUtils.browserLoaded(browser);
    await SpecialPowers.spawn(browser, [], () => {
      ok(content.document.getElementById("ok"), "Initial page should load");

      info("Clicking on link to check referrer");
      content.document.getElementById("check_referrer").click();
    });
    await loadPromise;

    await SpecialPowers.spawn(browser, [], () => {
      ok(
        content.document.getElementById("ok"),
        "Page should load when checking referrer"
      );

      info("Clicking on fragment link");
      content.document.getElementById("fragment").click();
    });

    info("Reloading tab");
    loadPromise = BrowserTestUtils.browserLoaded(browser);
    await BrowserTestUtils.reloadTab(gBrowser.selectedTab);
    await loadPromise;

    await SpecialPowers.spawn(browser, [], () => {
      ok(
        content.document.getElementById("ok"),
        "Page should load when checking referrer after fragment navigation and reload"
      );

      info("Clicking on push_state button");
      content.document.getElementById("push_state").click();
    });

    info("Reloading tab");
    loadPromise = BrowserTestUtils.browserLoaded(browser);
    await BrowserTestUtils.reloadTab(gBrowser.selectedTab);
    await loadPromise;

    await SpecialPowers.spawn(browser, [], () => {
      ok(
        content.document.getElementById("ok"),
        "Page should load when checking referrer after history.pushState and reload"
      );
    });
  });
});
