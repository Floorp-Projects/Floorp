/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  waitForExplicitFinish();
  try {
    let cm = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
    cm.getCategoryEntry("healthreport-js-provider-default", "SearchesProvider");
  } catch (ex) {
    // Health Report disabled, or no SearchesProvider.
    ok(true, "Firefox Health Report is not enabled.");
    finish();
    return;
  }

  let reporter = Cc["@mozilla.org/datareporting/service;1"]
                   .getService()
                   .wrappedJSObject
                   .healthReporter;
  ok(reporter, "Health Reporter available.");
  reporter.onInit().then(function onInit() {
    let provider = reporter.getProvider("org.mozilla.searches");
    ok(provider, "Searches provider is available.");
    let m = provider.getMeasurement("counts", 3);

    m.getValues().then(function onData(data) {
      let now = new Date();
      let oldCount = 0;

      // This will to be need changed if default search engine is not Google.
      let field = "google.urlbar";

      if (data.days.hasDay(now)) {
        let day = data.days.getDay(now);
        if (day.has(field)) {
          oldCount = day.get(field);
        }
      }

      let tab = gBrowser.addTab();
      gBrowser.selectedTab = tab;

      let searchStr = "firefox health report";
      let expectedURL = Services.search.currentEngine.
                        getSubmission(searchStr, "", "keyword").uri.spec;

      // Expect the search URL to load but stop it as soon as it starts.
      let loadPromise = waitForDocLoadAndStopIt(expectedURL);

      // Meanwhile, poll for the new measurement.
      let count = 0;
      let measurementDeferred = Promise.defer();
      function getNewMeasurement() {
        if (count++ >= 10) {
          ok(false, "Timed out waiting for new measurement");
          measurementDeferred.resolve();
          return;
        }
        m.getValues().then(function onData(data) {
          if (data.days.hasDay(now)) {
            let day = data.days.getDay(now);
            if (day.has(field)) {
              let newCount = day.get(field);
              if (newCount > oldCount) {
                is(newCount, oldCount + 1,
                   "Exactly one search has been recorded.");
                measurementDeferred.resolve();
                return;
              }
            }
          }
          executeSoon(getNewMeasurement);
        });
      }
      executeSoon(getNewMeasurement);

      // Trigger the search.
      gURLBar.value = searchStr;
      gURLBar.handleCommand();

      // Wait for the page load and new measurement.
      Promise.all([loadPromise, measurementDeferred.promise]).then(() => {
        gBrowser.removeTab(tab);
        finish();
      });
    });
  });
}

