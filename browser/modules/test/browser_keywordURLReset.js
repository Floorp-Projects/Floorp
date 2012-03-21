/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let newTab = gBrowser.selectedTab = gBrowser.addTab("about:blank", {skipAnimation: true});
  registerCleanupFunction(function () {
    gBrowser.removeTab(newTab);
    Services.prefs.clearUserPref("keyword.URL");
    Services.prefs.clearUserPref("browser.search.defaultenginename");
  });

  function onKeywordPrefUse(cb) {
    Services.obs.addObserver(function obs() {
      Services.obs.removeObserver(obs, "defaultURIFixup-using-keyword-pref");
      executeSoon(function () {
        let nbox = gBrowser.getNotificationBox();
        let notification = nbox.getNotificationWithValue("keywordURL-reset");
        cb(notification);
      });
    }, "defaultURIFixup-using-keyword-pref", false);
  }

  function testQuery(cb) {
    onKeywordPrefUse(cb);
    gURLBar.focus();
    gURLBar.value = "foo bar";
    gURLBar.handleCommand();
  }

  function setKeywordURL(value) {
    Services.prefs.setCharPref("keyword.URL", value);
    let keywordURI = XULBrowserWindow._uriFixup.keywordToURI("foo bar");
    is(keywordURI.spec, value + "foo+bar", "keyword.URL was set to " + value);
  }

  // Before modifying any prefs, get a submission object for this build's
  // actual default engine for verification of the "Reset" functionality.
  let originalDefaultEngine = Services.search.originalDefaultEngine;
  let defaultSubmission = originalDefaultEngine.getSubmission("foo bar", "application/x-moz-keywordsearch");
  if (!defaultSubmission)
    defaultSubmission = originalDefaultEngine.getSubmission("foo bar");

  var tests = [
    {
      name: "similar URL that shouldn't trigger prompt",
      setup: function () {
        setKeywordURL(defaultSubmission.uri.prePath + "/othersearch");
      },
      check: function (notification) {
        ok(!notification, "didn't get a search reset notification");
      }
    },
    {
      name: "URL that should trigger prompt",
      setup: function () {
        // First clear any values from the previous test so that we can verify
        // that the dummy default engine we're adding below has an effect.
        Services.prefs.clearUserPref("keyword.URL");

        // Add a dummy "default engine" to verify that the "reset" actually
        // resets the originalDefaultEngine too (and not just keyword.URL)
        Services.search.addEngineWithDetails("TestEngine", "", "", "", "GET", "http://mochi.test/test?q={searchTerms}");
        Services.prefs.setCharPref("browser.search.defaultenginename", "TestEngine");

        // Check that it was added successfully
        let engine = Services.search.getEngineByName("TestEngine");
        ok(engine, "added engine successfully");
        registerCleanupFunction(function () {
          Services.search.removeEngine(engine);
        });
        is(Services.search.originalDefaultEngine, engine, "engine is now the originalDefaultEngine");
        let keywordURI = XULBrowserWindow._uriFixup.keywordToURI("foo bar");
        is(keywordURI.spec, "http://mochi.test/test?q=foo+bar", "default engine affects keywordToURI");

        setKeywordURL("http://invalid.foo/search/");
      },
      check: function (notification) {
        ok(notification, "got a search reset notification");

        // Press the "reset" button
        notification.getElementsByTagName("button").item(0).click();

        // Check that the reset worked
        let keywordURI = XULBrowserWindow._uriFixup.keywordToURI("foo bar");
        is(keywordURI.spec, defaultSubmission.uri.spec,
           "keyword.URL came from original default engine after reset");
      }
    }
  ];

  function nextTest() {
    let test = tests.shift();
    if (!test) {
      finish();
      return;
    }

    info("Running test: " + test.name);
    test.setup();
    testQuery(function (notification) {
      test.check(notification);
      executeSoon(nextTest);
    });
  }
  
  nextTest();
}

