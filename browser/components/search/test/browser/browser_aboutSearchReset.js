/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TELEMETRY_RESULT_ENUM = {
  RESTORED_DEFAULT: 0,
  KEPT_CURRENT: 1,
  CHANGED_ENGINE: 2,
  CLOSED_PAGE: 3,
  OPENED_SETTINGS: 4,
};

const kSearchStr = "a search";
const kSearchPurpose = "searchbar";

const kTestEngine = "testEngine.xml";

const kStatusPref = "browser.search.reset.status";

function checkTelemetryRecords(expectedValue) {
  let histogram = Services.telemetry.getHistogramById("SEARCH_RESET_RESULT");
  let snapshot = histogram.snapshot();
  if (expectedValue != null) {
    Assert.deepEqual(snapshot.values[expectedValue], 1,
                     "histogram has expected content");
  } else {
    Assert.deepEqual(!!snapshot.values[expectedValue], false,
                     "histogram has expected content");
  }
  histogram.clear();
}

function promiseStoppedLoad(expectedURL) {
  return new Promise(resolve => {
    let browser = gBrowser.selectedBrowser;
    let original = browser.loadURI;
    browser.loadURI = function(URI) {
      if (URI == expectedURL) {
        browser.loadURI = original;
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
  async run() {
    let engine = await promiseNewEngine(kTestEngine, {setAsCurrent: true});

    let expectedURL = engine.
                      getSubmission(kSearchStr, null, kSearchPurpose).
                      uri.spec;

    let rawEngine = engine.wrappedJSObject;
    let initialHash = rawEngine.getAttr("loadPathHash");
    rawEngine.setAttr("loadPathHash", "broken");
    Services.prefs.setCharPref(kStatusPref, "pending");

    let loadPromise = promiseStoppedLoad(expectedURL);
    await BrowserTestUtils.synthesizeMouseAtCenter("#searchResetKeepCurrent", {},
                                                   gBrowser.selectedBrowser);
    await loadPromise;

    is(engine, Services.search.defaultEngine,
       "the custom engine is still default");
    is(rawEngine.getAttr("loadPathHash"), initialHash,
       "the loadPathHash has been fixed");

    checkTelemetryRecords(TELEMETRY_RESULT_ENUM.KEPT_CURRENT);
    is(Services.prefs.getCharPref(kStatusPref), "declined");
  },
},

{
  desc: "Test the 'Restore Search Defaults' button.",
  async run() {
    let currentEngine = Services.search.defaultEngine;
    let originalEngine = Services.search.originalDefaultEngine;
    let browser = gBrowser.selectedBrowser;
    let defaultEngineSpanText =
      await ContentTask.spawn(browser, null, async () => {
        return content.document.getElementById("defaultEngine").textContent;
      });

    is(defaultEngineSpanText, originalEngine.name,
       "the name of the original default engine is displayed");

    let expectedURL = originalEngine.
                      getSubmission(kSearchStr, null, kSearchPurpose).
                      uri.spec;
    let loadPromise = promiseStoppedLoad(expectedURL);

    await ContentTask.spawn(browser, null, async () => {
      let button = content.document.getElementById("searchResetChangeEngine");
      Assert.equal(content.document.activeElement, button,
                   "the 'Change Search Engine' button is focused");
    });

    Services.prefs.setCharPref(kStatusPref, "pending");
    await BrowserTestUtils.synthesizeMouseAtCenter("#searchResetChangeEngine",
                                                   {}, browser);
    await loadPromise;

    is(originalEngine, Services.search.defaultEngine,
       "the default engine is back to the original one");

    checkTelemetryRecords(TELEMETRY_RESULT_ENUM.RESTORED_DEFAULT);
    is(Services.prefs.getCharPref(kStatusPref), "accepted");
    Services.search.defaultEngine = currentEngine;
  },
},

{
  desc: "Click the settings link.",
  async run() {
    Services.prefs.setCharPref(kStatusPref, "pending");
    let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser,
                                                     false,
                                                     "about:preferences#search");
    let browser = gBrowser.selectedBrowser;
    await BrowserTestUtils.synthesizeMouseAtCenter("#linkSettingsPage",
                                                   {}, browser);
    await loadPromise;

    checkTelemetryRecords(TELEMETRY_RESULT_ENUM.OPENED_SETTINGS);
    is(Services.prefs.getCharPref(kStatusPref), "customized");
  },
},

{
  desc: "Load another page without clicking any of the buttons.",
  async run() {
    Services.prefs.setCharPref(kStatusPref, "pending");
    await promiseTabLoadEvent(gBrowser.selectedTab, "about:mozilla");

    checkTelemetryRecords(TELEMETRY_RESULT_ENUM.CLOSED_PAGE);
    is(Services.prefs.getCharPref(kStatusPref), "pending");
  },
},

];

function test() {
  waitForExplicitFinish();
  (async function() {
    let oldCanRecord = Services.telemetry.canRecordExtended;
    Services.telemetry.canRecordExtended = true;
    checkTelemetryRecords();

    for (let testCase of gTests) {
      info(testCase.desc);

      // Create a tab to run the test.
      let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");

      // Start loading about:searchreset and wait for it to complete.
      let url = "about:searchreset?data=" + encodeURIComponent(kSearchStr) +
                "&purpose=" + kSearchPurpose;
      await promiseTabLoadEvent(tab, url);

      info("Running test");
      await testCase.run();

      info("Cleanup");
      gBrowser.removeCurrentTab();
    }

    Services.prefs.clearUserPref(kStatusPref);
    Services.telemetry.canRecordExtended = oldCanRecord;
  })().then(finish, ex => {
    ok(false, "Unexpected Exception: " + ex);
    finish();
  });
}
