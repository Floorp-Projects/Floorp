/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  waitForExplicitFinish();

  try {
    let cm = Components.classes["@mozilla.org/categorymanager;1"]
                       .getService(Components.interfaces.nsICategoryManager);
    cm.getCategoryEntry("healthreport-js-provider-default", "SearchesProvider");
  } catch (ex) {
    // Health Report disabled, or no SearchesProvider.
    // We need a test or else we'll be marked as failure.
    ok(true, "Firefox Health Report is not enabled.");
    finish();
    return;
  }

  function testFHR() {
    let reporter = Components.classes["@mozilla.org/datareporting/service;1"]
                                     .getService()
                                     .wrappedJSObject
                                     .healthReporter;
    ok(reporter, "Health Reporter available.");
    reporter.onInit().then(function onInit() {
      let provider = reporter.getProvider("org.mozilla.searches");
      let m = provider.getMeasurement("counts", 2);

      m.getValues().then(function onData(data) {
        let now = new Date();
        let oldCount = 0;

        // Foo engine goes into "other" bucket.
        let field = "other.searchbar";

        if (data.days.hasDay(now)) {
          let day = data.days.getDay(now);
          if (day.has(field)) {
            oldCount = day.get(field);
          }
        }

        // Now perform a search and ensure the count is incremented.
        let tab = gBrowser.addTab();
        gBrowser.selectedTab = tab;
        let searchBar = BrowserSearch.searchBar;

        searchBar.value = "firefox health report";
        searchBar.focus();

        function afterSearch() {
          searchBar.value = "";
          gBrowser.removeTab(tab);

          m.getValues().then(function onData(data) {
            ok(data.days.hasDay(now), "Have data for today.");
            let day = data.days.getDay(now);

            is(day.get(field), oldCount + 1, "Performing a search increments FHR count by 1.");

            let engine = Services.search.getEngineByName("Foo");
            Services.search.removeEngine(engine);
          });
        }

        EventUtils.synthesizeKey("VK_RETURN", {});
        executeSoon(afterSearch);
      });
    });
  }

  function observer(subject, topic, data) {
    switch (data) {
      case "engine-added":
        let engine = Services.search.getEngineByName("Foo");
        ok(engine, "Engine was added.");
        Services.search.currentEngine = engine;
        break;

      case "engine-current":
        is(Services.search.currentEngine.name, "Foo", "Current engine is Foo");
        testFHR();
        break;

      case "engine-removed":
        Services.obs.removeObserver(observer, "browser-search-engine-modified");
        finish();
        break;
    }
  }

  Services.obs.addObserver(observer, "browser-search-engine-modified", false);
  Services.search.addEngine("http://mochi.test:8888/browser/browser/components/search/test/testEngine.xml",
                            Ci.nsISearchEngine.DATA_XML,
                            "data:image/x-icon,%00",
                            false);

}

