/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");

ignoreAllUncaughtExceptions();

add_task(async function setup() {
  // The following prefs would affect tests so make sure to disable them
  // before any tests start.
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.newtabpage.activity-stream.aboutHome.enabled", false],
  ]});
});

// The following two tests need to be skipped for the time being, since we're
// no longer showing the launcher options on about:home. When we remove about:home
// and all of it's code, we can delete these tests
add_task(async function() {
  info("Pressing Space while the Addons button is focused should activate it");

  // Skip this test on Mac, because Space doesn't activate the button there.
  if (AppConstants.platform == "macosx") {
    return;
  }

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, async function(browser) {
    info("Waiting for about:addons tab to open...");
    let promiseTabLoaded = BrowserTestUtils.browserLoaded(browser, false, "about:addons");

    await ContentTask.spawn(browser, null, async function() {
      let addOnsButton = content.document.getElementById("addons");
      addOnsButton.focus();
    });
    await BrowserTestUtils.synthesizeKey(" ", {}, browser);

    await promiseTabLoaded;
    is(browser.currentURI.spec, "about:addons",
      "Should have seen the about:addons tab");
  });
}).skip();

add_task(async function() {
  info("Sync button should open about:preferences#sync");

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, async function(browser) {
    let oldOpenPrefs = window.openPreferences;
    let openPrefsPromise = new Promise(resolve => {
      window.openPreferences = function(pane, params) {
        resolve({ pane, params });
      };
    });

    await BrowserTestUtils.synthesizeMouseAtCenter("#sync", {}, browser);

    let result = await openPrefsPromise;
    window.openPreferences = oldOpenPrefs;

    is(result.pane, "paneSync", "openPreferences should be called with paneSync");
    is(result.params.urlParams.entrypoint, "abouthome",
      "openPreferences should be called with abouthome entrypoint");
  });
}).skip();

add_task(async function() {
  info("Pressing any key should focus the search box in the page, and send the key to it");

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, async function(browser) {
    await BrowserTestUtils.synthesizeMouseAtCenter("#brandLogo", {}, browser);

    await ContentTask.spawn(browser, null, async function() {
      let doc = content.document;
      isnot(doc.getElementById("searchText"), doc.activeElement,
        "Search input should not be the active element.");
    });

    await BrowserTestUtils.synthesizeKey("a", {}, browser);

    await ContentTask.spawn(browser, null, async function() {
      let doc = content.document;
      let searchInput = doc.getElementById("searchText");
      await ContentTaskUtils.waitForCondition(() => doc.activeElement === searchInput,
        "Search input should be the active element.");
      is(searchInput.value, "a", "Search input should be 'a'.");
    });
  });
});
