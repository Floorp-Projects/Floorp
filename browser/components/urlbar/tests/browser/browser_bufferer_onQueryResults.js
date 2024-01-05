/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test covers a race condition of input events followed by Enter.
// The test is putting the event bufferer in a situation where a new query has
// already results in the context object, but onQueryResults has not been
// invoked yet. The EventBufferer should wait for onQueryResults to proceed,
// otherwise the view cannot yet contain the updated query string and we may
// end up searching for a partial string.

add_setup(async function () {
  sandbox = sinon.createSandbox();
  await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + "searchSuggestionEngine.xml",
    setAsDefault: true,
  });
  // To reproduce the race condition it's important to disable any provider
  // having `deferUserSelection` == true;
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.engines", false]],
  });
  await PlacesUtils.history.clear();
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
    sandbox.restore();
  });
});

add_task(async function test() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:robots",
  });

  let defer = Promise.withResolvers();
  let waitFirstSearchResults = Promise.withResolvers();
  let count = 0;
  let original = gURLBar.controller.notify;
  sandbox.stub(gURLBar.controller, "notify").callsFake(async (msg, context) => {
    if (context?.deferUserSelectionProviders.size) {
      Assert.ok(false, "Any provider deferring selection should be disabled");
    }
    if (msg == "onQueryResults") {
      waitFirstSearchResults.resolve();
      count++;
    }
    // Delay any events after the second onQueryResults call.
    if (count >= 2) {
      await defer.promise;
    }
    return original.call(gURLBar.controller, msg, context);
  });

  gURLBar.focus();
  gURLBar.select();
  EventUtils.synthesizeKey("t", {});
  await waitFirstSearchResults.promise;
  EventUtils.synthesizeKey("e", {});

  let promiseLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  EventUtils.synthesizeKey("KEY_Enter", {});

  let context = await UrlbarTestUtils.promiseSearchComplete(window);
  await TestUtils.waitForCondition(
    () => context.results.length,
    "Waiting for any result in the QueryContext"
  );
  info("Simulate a request to replay deferred events at this point");
  gURLBar.eventBufferer.replayDeferredEvents(true);

  defer.resolve();
  await promiseLoaded;

  let expectedURL = UrlbarPrefs.isPersistedSearchTermsEnabled()
    ? "http://mochi.test:8888/?terms=" + gURLBar.value
    : gURLBar.untrimmedValue;
  Assert.equal(gBrowser.selectedBrowser.currentURI.spec, expectedURL);

  BrowserTestUtils.removeTab(tab);
  sandbox.restore();
});
