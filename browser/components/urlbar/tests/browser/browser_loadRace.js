/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test is for testing races of loading the Urlbar when loading shortcuts.
// For example, ensuring that if a search query is entered, but something causes
// a page load whilst we're getting the search url, then we don't handle the
// original search query.

add_setup(async function () {
  sandbox = sinon.createSandbox();

  registerCleanupFunction(async () => {
    sandbox.restore();
  });
});

async function checkShortcutLoading(modifierKeys) {
  let deferred = PromiseUtils.defer();
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:robots",
  });

  // We stub getHeuristicResultFor to guarentee it doesn't resolve until after
  // we've loaded a new page.
  let original = UrlbarUtils.getHeuristicResultFor;
  sandbox
    .stub(UrlbarUtils, "getHeuristicResultFor")
    .callsFake(async searchString => {
      await deferred.promise;
      return original.call(this, searchString);
    });

  // This load will be blocked until the deferred is resolved.
  // Use a string that will be interepreted as a local URL to avoid hitting the
  // network.
  gURLBar.focus();
  gURLBar.value = "example.com";
  gURLBar.userTypedValue = true;
  EventUtils.synthesizeKey("KEY_Enter", modifierKeys);

  Assert.ok(
    UrlbarUtils.getHeuristicResultFor.calledOnce,
    "should have called getHeuristicResultFor"
  );

  // Now load a different page.
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, "about:license");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  Assert.equal(gBrowser.visibleTabs.length, 2, "Should have 2 tabs");

  // Now that the new page has loaded, unblock the previous urlbar load.
  deferred.resolve();
  if (modifierKeys) {
    let openedTab = await new Promise(resolve => {
      window.addEventListener(
        "TabOpen",
        event => {
          resolve(event.target);
        },
        { once: true }
      );
    });
    await BrowserTestUtils.browserLoaded(openedTab.linkedBrowser);
    Assert.ok(
      openedTab.linkedBrowser.currentURI.spec.includes("example.com"),
      "Should have attempted to open the shortcut page"
    );
    BrowserTestUtils.removeTab(openedTab);
  }

  Assert.equal(
    tab.linkedBrowser.currentURI.spec,
    "about:license",
    "Tab url should not have changed"
  );
  Assert.equal(gBrowser.visibleTabs.length, 2, "Should still have 2 tabs");

  BrowserTestUtils.removeTab(tab);
  sandbox.restore();
}

add_task(async function test_location_change_stops_load() {
  await checkShortcutLoading();
});

add_task(async function test_opening_different_tab_with_location_change() {
  await checkShortcutLoading({ altKey: true });
});
