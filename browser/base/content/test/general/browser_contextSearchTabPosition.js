/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  function tabAdded(event) {
    let tab = event.target;
    tabs.push(tab);
  }

  let tabs = [];

  let container = gBrowser.tabContainer;
  container.addEventListener("TabOpen", tabAdded, false);

  gBrowser.addTab("about:blank");
  BrowserSearch.loadSearchFromContext("mozilla");
  BrowserSearch.loadSearchFromContext("firefox");

  is(tabs[0], gBrowser.tabs[3], "blank tab has been pushed to the end");
  is(tabs[1], gBrowser.tabs[1], "first search tab opens next to the current tab");
  is(tabs[2], gBrowser.tabs[2], "second search tab opens next to the first search tab");

  container.removeEventListener("TabOpen", tabAdded, false);
  tabs.forEach(gBrowser.removeTab, gBrowser);

  try {
    let cm = Components.classes["@mozilla.org/categorymanager;1"]
                       .getService(Components.interfaces.nsICategoryManager);
    cm.getCategoryEntry("healthreport-js-provider-default", "SearchesProvider");
  } catch (ex) {
    // Health Report disabled, or no SearchesProvider.
    finish();
    return;
  }

  let reporter = Components.classes["@mozilla.org/datareporting/service;1"]
                                   .getService()
                                   .wrappedJSObject
                                   .healthReporter;

  // reporter should always be available in automation.
  ok(reporter, "Health Reporter available.");
  reporter.onInit().then(function onInit() {
    let provider = reporter.getProvider("org.mozilla.searches");
    ok(provider, "Searches provider is available.");

    let m = provider.getMeasurement("counts", 3);
    m.getValues().then(function onValues(data) {
      let now = new Date();
      ok(data.days.hasDay(now), "Have data for today.");
      let day = data.days.getDay(now);

      // Will need to be changed if Google isn't the default search engine.
      // Note: geoSpecificDefaults are disabled for mochitests, so this is the
      // non-US en-US default.
      let defaultProviderID = "google";
      let field = defaultProviderID + ".contextmenu";
      ok(day.has(field), "Have search recorded for context menu.");

      // If any other mochitests perform a context menu search, this will fail.
      // The solution will be to look up count at test start and ensure it is
      // incremented by two.
      is(day.get(field), 2, "2 searches recorded in FHR.");
      finish();
    });
  });
}
