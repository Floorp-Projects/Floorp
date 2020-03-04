/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test is for testing races of loading the Urlbar when loading shortcuts.
// For example, ensuring that if a search query is entered, but something causes
// a page load whilst we're getting the search url, then we don't handle the
// original search query.

add_task(async function setup() {
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

  // We stub getShortcutOrURIAndPostData so that we can guarentee it doesn't resolve
  // until after we've loaded a new page.
  sandbox
    .stub(UrlbarUtils, "getShortcutOrURIAndPostData")
    .callsFake(() => deferred.promise);

  gURLBar.focus();
  gURLBar.value = "search";
  gURLBar.userTypedValue = true;
  EventUtils.synthesizeKey("KEY_Enter", modifierKeys);

  Assert.ok(
    UrlbarUtils.getShortcutOrURIAndPostData.calledOnce,
    "should have called getShortcutOrURIAndPostData"
  );

  BrowserTestUtils.loadURI(tab.linkedBrowser, "about:license");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  let openedTab;
  function listener(event) {
    openedTab = event.target;
  }
  window.addEventListener("TabOpen", listener);

  deferred.resolve({
    url: "https://example.com/1/",
    postData: {},
    mayInheritPrincipal: true,
  });

  await deferred.promise;

  if (modifierKeys) {
    Assert.ok(openedTab, "Should have attempted to open the shortcut page");
    BrowserTestUtils.removeTab(openedTab);
  } else {
    Assert.ok(
      !openedTab,
      "Should have not attempted to open the shortcut page"
    );
  }

  window.removeEventListener("TabOpen", listener);
  BrowserTestUtils.removeTab(tab);
  sandbox.restore();
}

add_task(async function test_location_change_stops_load() {
  await checkShortcutLoading();
});

add_task(async function test_opening_different_tab_with_location_change() {
  await checkShortcutLoading({ altKey: true });
});
