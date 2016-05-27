/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TELEMETRY_RESULT_ENUM = {
  RESTORED_DEFAULT: 0,
  KEPT_CURRENT: 1,
  CHANGED_ENGINE: 2,
  CLOSED_PAGE: 3
};

const kSearchStr = "a search";
const kSearchPurpose = "searchbar";

const kTestEngine = "testEngine.xml";

function checkTelemetryRecords(expectedValue) {
  let histogram = Services.telemetry.getHistogramById("SEARCH_RESET_RESULT");
  let snapshot = histogram.snapshot();
  // The probe is declared with 5 values, but we get 6 back from .counts
  let expectedCounts = [0, 0, 0, 0, 0, 0];
  if (expectedValue != null) {
    expectedCounts[expectedValue] = 1;
  }
  Assert.deepEqual(snapshot.counts, expectedCounts,
                   "histogram has expected content");
  histogram.clear();
}

function promiseStoppedLoad(expectedURL) {
  return new Promise(resolve => {
    let browser = gBrowser.selectedBrowser;
    let original = browser.loadURIWithFlags;
    browser.loadURIWithFlags = function(URI) {
      if (URI == expectedURL) {
        browser.loadURIWithFlags = original;
        ok(true, "loaded expected url: " + URI);
        resolve();
        return;
      }

      original.apply(browser, arguments);
    };
  });
}

var gTests = [

{
  desc: "Test the 'Keep Current Settings' button.",
  run: function* () {
    let engine = yield promiseNewEngine(kTestEngine, {setAsCurrent: true});

    let expectedURL = engine.
                      getSubmission(kSearchStr, null, kSearchPurpose).
                      uri.spec;

    let rawEngine = engine.wrappedJSObject;
    let initialHash = rawEngine.getAttr("loadPathHash");
    rawEngine.setAttr("loadPathHash", "broken");

    let loadPromise = promiseStoppedLoad(expectedURL);
    gBrowser.contentDocument.getElementById("searchResetKeepCurrent").click();
    yield loadPromise;

    is(engine, Services.search.currentEngine,
       "the custom engine is still default");
    is(rawEngine.getAttr("loadPathHash"), initialHash,
       "the loadPathHash has been fixed");

    checkTelemetryRecords(TELEMETRY_RESULT_ENUM.KEPT_CURRENT);
  }
},

{
  desc: "Test the 'Restore Search Defaults' button.",
  run: function* () {
    let currentEngine = Services.search.currentEngine;
    let originalEngine = Services.search.originalDefaultEngine;
    let expectedURL = originalEngine.
                      getSubmission(kSearchStr, null, kSearchPurpose).
                      uri.spec;

    let loadPromise = promiseStoppedLoad(expectedURL);
    let doc = gBrowser.contentDocument;
    let button = doc.getElementById("searchResetChangeEngine");
    is(doc.activeElement, button,
       "the 'Change Search Engine' button is focused");
    button.click();
    yield loadPromise;

    is(originalEngine, Services.search.currentEngine,
       "the default engine is back to the original one");

    checkTelemetryRecords(TELEMETRY_RESULT_ENUM.RESTORED_DEFAULT);
    Services.search.currentEngine = currentEngine;
  }
},

{
  desc: "Test the engine selector drop down.",
  run: function* () {
    let originalEngineName = Services.search.originalDefaultEngine.name;

    let doc = gBrowser.contentDocument;
    let list = doc.getElementById("defaultEngine");
    is(list.value, originalEngineName,
       "the default selection of the dropdown is the original default engine");

    let defaultEngines = Services.search.getDefaultEngines();
    is(list.childNodes.length, defaultEngines.length,
       "the dropdown has the correct count of engines");

    // Select an engine that isn't the original default one.
    let engine;
    for (let i = 0; i < defaultEngines.length; ++i) {
      if (defaultEngines[i].name != originalEngineName) {
        engine = defaultEngines[i];
        engine.hidden = true;
        break;
      }
    }
    list.value = engine.name;

    let expectedURL = engine.getSubmission(kSearchStr, null, kSearchPurpose)
                            .uri.spec;
    let loadPromise = promiseStoppedLoad(expectedURL);
    doc.getElementById("searchResetChangeEngine").click();
    yield loadPromise;

    ok(!engine.hidden, "the selected engine has been unhidden");
    is(engine, Services.search.currentEngine,
       "the current engine is what was selected in the drop down");

    checkTelemetryRecords(TELEMETRY_RESULT_ENUM.CHANGED_ENGINE);
  }
},

{
  desc: "Load another page without clicking any of the buttons.",
  run: function* () {
    yield promiseTabLoadEvent(gBrowser.selectedTab, "about:mozilla");

    checkTelemetryRecords(TELEMETRY_RESULT_ENUM.CLOSED_PAGE);
  }
},

];

function test()
{
  waitForExplicitFinish();
  Task.spawn(function () {
    let oldCanRecord = Services.telemetry.canRecordExtended;
    Services.telemetry.canRecordExtended = true;
    checkTelemetryRecords();

    for (let test of gTests) {
      info(test.desc);

      // Create a tab to run the test.
      let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");

      // Start loading about:searchreset and wait for it to complete.
      let url = "about:searchreset?data=" + encodeURIComponent(kSearchStr) +
                "&purpose=" + kSearchPurpose;
      yield promiseTabLoadEvent(tab, url);

      info("Running test");
      yield test.run();

      info("Cleanup");
      gBrowser.removeCurrentTab();
    }

    Services.telemetry.canRecordExtended = oldCanRecord;
  }).then(finish, ex => {
    ok(false, "Unexpected Exception: " + ex);
    finish();
  });
}
