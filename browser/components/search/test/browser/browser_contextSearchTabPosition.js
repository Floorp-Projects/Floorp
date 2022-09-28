/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let engine;

add_setup(async function() {
  engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "testEngine.xml"
  );
  const current = await Services.search.getDefault();
  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  registerCleanupFunction(async () => {
    await Services.search.setDefault(
      current,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
  });
});

add_task(async function test() {
  let histogramKey = "other-" + engine.name + ".contextmenu";
  let numSearchesBefore = 0;

  try {
    let hs = Services.telemetry
      .getKeyedHistogramById("SEARCH_COUNTS")
      .snapshot();
    if (histogramKey in hs) {
      numSearchesBefore = hs[histogramKey].sum;
    }
  } catch (ex) {
    // No searches performed yet, not a problem, |numSearchesBefore| is 0.
  }

  let tabs = [];
  let tabsLoadedDeferred = new Deferred();

  function tabAdded(event) {
    let tab = event.target;
    tabs.push(tab);

    // We wait for the blank tab and the two context searches tabs to open.
    if (tabs.length == 3) {
      tabsLoadedDeferred.resolve();
    }
  }

  let container = gBrowser.tabContainer;
  container.addEventListener("TabOpen", tabAdded);

  BrowserTestUtils.addTab(gBrowser, "about:blank");
  BrowserSearch.loadSearchFromContext(
    "mozilla",
    false,
    Services.scriptSecurityManager.getSystemPrincipal(),
    Services.scriptSecurityManager.getSystemPrincipal().csp,
    new MouseEvent("click")
  );
  BrowserSearch.loadSearchFromContext(
    "firefox",
    false,
    Services.scriptSecurityManager.getSystemPrincipal(),
    Services.scriptSecurityManager.getSystemPrincipal().csp,
    new MouseEvent("click")
  );

  // Wait for all the tabs to open.
  await tabsLoadedDeferred.promise;

  is(tabs[0], gBrowser.tabs[3], "blank tab has been pushed to the end");
  is(
    tabs[1],
    gBrowser.tabs[1],
    "first search tab opens next to the current tab"
  );
  is(
    tabs[2],
    gBrowser.tabs[2],
    "second search tab opens next to the first search tab"
  );

  container.removeEventListener("TabOpen", tabAdded);
  tabs.forEach(gBrowser.removeTab, gBrowser);

  // Make sure that the context searches are correctly recorded in telemetry.
  // Telemetry is not updated synchronously here, we must wait for it.
  await TestUtils.waitForCondition(() => {
    let hs = Services.telemetry
      .getKeyedHistogramById("SEARCH_COUNTS")
      .snapshot();
    return histogramKey in hs && hs[histogramKey].sum == numSearchesBefore + 2;
  }, "The histogram must contain the correct search count");
});

function Deferred() {
  this.promise = new Promise((resolve, reject) => {
    this.resolve = resolve;
    this.reject = reject;
  });
}
