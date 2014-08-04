/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AboutHomeUtils",
  "resource:///modules/AboutHome.jsm");

const TEST_CONTENT_HELPER = "chrome://mochitests/content/browser/browser/base/content/test/general/aboutHome_content_script.js";
let gRightsVersion = Services.prefs.getIntPref("browser.rights.version");

registerCleanupFunction(function() {
  // Ensure we don't pollute prefs for next tests.
  Services.prefs.clearUserPref("network.cookies.cookieBehavior");
  Services.prefs.clearUserPref("network.cookie.lifetimePolicy");
  Services.prefs.clearUserPref("browser.rights.override");
  Services.prefs.clearUserPref("browser.rights." + gRightsVersion + ".shown");
});

let gTests = [

{
  desc: "Check that clearing cookies does not clear storage",
  setup: function ()
  {
    Cc["@mozilla.org/observer-service;1"]
      .getService(Ci.nsIObserverService)
      .notifyObservers(null, "cookie-changed", "cleared");
  },
  run: function (aSnippetsMap)
  {
    isnot(aSnippetsMap.get("snippets-last-update"), null,
          "snippets-last-update should have a value");
  }
},

{
  desc: "Check default snippets are shown",
  setup: function () { },
  run: function ()
  {
    let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;
    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element")
    is(snippetsElt.getElementsByTagName("span").length, 1,
       "A default snippet is present.");
  }
},

{
  desc: "Check default snippets are shown if snippets are invalid xml",
  setup: function (aSnippetsMap)
  {
    // This must be some incorrect xhtml code.
    aSnippetsMap.set("snippets", "<p><b></p></b>");
  },
  run: function (aSnippetsMap)
  {
    let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;

    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element");
    is(snippetsElt.getElementsByTagName("span").length, 1,
       "A default snippet is present.");

    aSnippetsMap.delete("snippets");
  }
},

{
  desc: "Check that search engine logo has alt text",
  setup: function () { },
  run: function ()
  {
    let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;

    let searchEngineLogoElt = doc.getElementById("searchEngineLogo");
    ok(searchEngineLogoElt, "Found search engine logo");

    let altText = searchEngineLogoElt.alt;
    ok(typeof altText == "string" && altText.length > 0,
       "Search engine logo's alt text is a nonempty string");

    isnot(altText, "undefined",
          "Search engine logo's alt text shouldn't be the string 'undefined'");
  }
},

// Disabled on Linux for intermittent issues with FHR, see Bug 945667.
{
  desc: "Check that performing a search fires a search event and records to " +
        "Firefox Health Report.",
  setup: function () { },
  run: function () {
    // Skip this test on Linux.
    if (navigator.platform.indexOf("Linux") == 0) { return; }

    try {
      let cm = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
      cm.getCategoryEntry("healthreport-js-provider-default", "SearchesProvider");
    } catch (ex) {
      // Health Report disabled, or no SearchesProvider.
      return Promise.resolve();
    }

    let numSearchesBefore = 0;
    let searchEventDeferred = Promise.defer();
    let doc = gBrowser.contentDocument;
    let engineName = doc.documentElement.getAttribute("searchEngineName");
    let mm = gBrowser.selectedTab.linkedBrowser.messageManager;

    mm.loadFrameScript(TEST_CONTENT_HELPER, false);

    mm.addMessageListener("AboutHomeTest:CheckRecordedSearch", function (msg) {
      let data = JSON.parse(msg.data);
      is(data.engineName, engineName, "Detail is search engine name");

      getNumberOfSearches(engineName).then(num => {
        is(num, numSearchesBefore + 1, "One more search recorded.");
        searchEventDeferred.resolve();
      });
    });

    // Get the current number of recorded searches.
    let searchStr = "a search";
    getNumberOfSearches(engineName).then(num => {
      numSearchesBefore = num;

      info("Perform a search.");
      doc.getElementById("searchText").value = searchStr;
      doc.getElementById("searchSubmit").click();
    });

    let expectedURL = Services.search.currentEngine.
                      getSubmission(searchStr, null, "homepage").
                      uri.spec;
    let loadPromise = waitForDocLoadAndStopIt(expectedURL);

    return Promise.all([searchEventDeferred.promise, loadPromise]);
  }
},

{
  desc: "Check snippets map is cleared if cached version is old",
  setup: function (aSnippetsMap)
  {
    aSnippetsMap.set("snippets", "test");
    aSnippetsMap.set("snippets-cached-version", 0);
  },
  run: function (aSnippetsMap)
  {
    ok(!aSnippetsMap.has("snippets"), "snippets have been properly cleared");
    ok(!aSnippetsMap.has("snippets-cached-version"),
       "cached-version has been properly cleared");
  }
},

{
  desc: "Check cached snippets are shown if cached version is current",
  setup: function (aSnippetsMap)
  {
    aSnippetsMap.set("snippets", "test");
  },
  run: function (aSnippetsMap)
  {
    let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;

    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element");
    is(snippetsElt.innerHTML, "test", "Cached snippet is present.");

    is(aSnippetsMap.get("snippets"), "test", "snippets still cached");
    is(aSnippetsMap.get("snippets-cached-version"),
       AboutHomeUtils.snippetsVersion,
       "cached-version is correct");
    ok(aSnippetsMap.has("snippets-last-update"), "last-update still exists");
  }
},

{
  desc: "Check if the 'Know Your Rights default snippet is shown when 'browser.rights.override' pref is set",
  beforeRun: function ()
  {
    Services.prefs.setBoolPref("browser.rights.override", false);
  },
  setup: function () { },
  run: function (aSnippetsMap)
  {
    let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;
    let showRights = AboutHomeUtils.showKnowYourRights;

    ok(showRights, "AboutHomeUtils.showKnowYourRights should be TRUE");

    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element");
    is(snippetsElt.getElementsByTagName("a")[0].href, "about:rights", "Snippet link is present.");

    Services.prefs.clearUserPref("browser.rights.override");
  }
},

{
  desc: "Check if the 'Know Your Rights default snippet is NOT shown when 'browser.rights.override' pref is NOT set",
  beforeRun: function ()
  {
    Services.prefs.setBoolPref("browser.rights.override", true);
  },
  setup: function () { },
  run: function (aSnippetsMap)
  {
    let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;
    let rightsData = AboutHomeUtils.knowYourRightsData;

    ok(!rightsData, "AboutHomeUtils.knowYourRightsData should be FALSE");

    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element");
    ok(snippetsElt.getElementsByTagName("a")[0].href != "about:rights", "Snippet link should not point to about:rights.");

    Services.prefs.clearUserPref("browser.rights.override");
  }
},

{
  desc: "Check that the search UI/ action is updated when the search engine is changed",
  setup: function() {},
  run: function()
  {
    let currEngine = Services.search.currentEngine;
    let unusedEngines = [].concat(Services.search.getVisibleEngines()).filter(x => x != currEngine);
    let searchbar = document.getElementById("searchbar");

    function checkSearchUI(engine) {
      let doc = gBrowser.selectedTab.linkedBrowser.contentDocument;
      let searchText = doc.getElementById("searchText");
      let logoElt = doc.getElementById("searchEngineLogo");
      let engineName = doc.documentElement.getAttribute("searchEngineName");

      is(engineName, engine.name, "Engine name should've been updated");

      if (!logoElt.parentNode.hidden) {
        is(logoElt.alt, engineName, "Alt text of logo image should match search engine name")
      } else {
        is(searchText.placeholder, engineName, "Placeholder text should match search engine name");
      }
    }
    // Do a sanity check that all attributes are correctly set to begin with
    checkSearchUI(currEngine);

    let deferred = Promise.defer();
    promiseBrowserAttributes(gBrowser.selectedTab).then(function() {
      // Test if the update propagated
      checkSearchUI(unusedEngines[0]);
      searchbar.currentEngine = currEngine;
      deferred.resolve();
    });

    // The following cleanup function will set currentEngine back to the previous
    // engine if we fail to do so above.
    registerCleanupFunction(function() {
      searchbar.currentEngine = currEngine;
    });
    // Set the current search engine to an unused one
    searchbar.currentEngine = unusedEngines[0];
    searchbar.select();
    return deferred.promise;
  }
},

{
  desc: "Check POST search engine support",
  setup: function() {},
  run: function()
  {
    let deferred = Promise.defer();
    let currEngine = Services.search.defaultEngine;
    let searchObserver = function search_observer(aSubject, aTopic, aData) {
      let engine = aSubject.QueryInterface(Ci.nsISearchEngine);
      info("Observer: " + aData + " for " + engine.name);

      if (aData != "engine-added")
        return;

      if (engine.name != "POST Search")
        return;

      // Ready to execute the tests!
      let needle = "Search for something awesome.";
      let document = gBrowser.selectedTab.linkedBrowser.contentDocument;
      let searchText = document.getElementById("searchText");

      // We're about to change the search engine. Once the change has
      // propagated to the about:home content, we want to perform a search.
      let mutationObserver = new MutationObserver(function (mutations) {
        for (let mutation of mutations) {
          if (mutation.attributeName == "searchEngineName") {
            searchText.value = needle;
            searchText.focus();
            EventUtils.synthesizeKey("VK_RETURN", {});
          }
        }
      });
      mutationObserver.observe(document.documentElement, { attributes: true });

      // Change the search engine, triggering the observer above.
      Services.search.defaultEngine = engine;

      registerCleanupFunction(function() {
        mutationObserver.disconnect();
        Services.search.removeEngine(engine);
        Services.search.defaultEngine = currEngine;
      });


      // When the search results load, check them for correctness.
      waitForLoad(function() {
        let loadedText = gBrowser.contentDocument.body.textContent;
        ok(loadedText, "search page loaded");
        is(loadedText, "searchterms=" + escape(needle.replace(/\s/g, "+")),
           "Search text should arrive correctly");
        deferred.resolve();
      });
    };
    Services.obs.addObserver(searchObserver, "browser-search-engine-modified", false);
    registerCleanupFunction(function () {
      Services.obs.removeObserver(searchObserver, "browser-search-engine-modified");
    });
    Services.search.addEngine("http://test:80/browser/browser/base/content/test/general/POSTSearchEngine.xml",
                              Ci.nsISearchEngine.DATA_XML, null, false);
    return deferred.promise;
  }
},

{
  desc: "Make sure that a page can't imitate about:home",
  setup: function () { },
  run: function (aSnippetsMap)
  {
    let deferred = Promise.defer();

    let browser = gBrowser.selectedTab.linkedBrowser;
    waitForLoad(() => {
      let button = browser.contentDocument.getElementById("settings");
      ok(button, "Found settings button in test page");
      button.click();

      // It may take a few turns of the event loop before the window
      // is displayed, so we wait.
      function check(n) {
        let win = Services.wm.getMostRecentWindow("Browser:Preferences");
        ok(!win, "Preferences window not showing");
        if (win) {
          win.close();
        }

        if (n > 0) {
          executeSoon(() => check(n-1));
        } else {
          deferred.resolve();
        }
      }

      check(5);
    });

    browser.loadURI("https://example.com/browser/browser/base/content/test/general/test_bug959531.html");
    return deferred.promise;
  }
},

];

function test()
{
  waitForExplicitFinish();
  requestLongerTimeout(2);
  ignoreAllUncaughtExceptions();

  Task.spawn(function () {
    for (let test of gTests) {
      info(test.desc);

      if (test.beforeRun)
        yield test.beforeRun();

      // Create a tab to run the test.
      let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");

      // Add an event handler to modify the snippets map once it's ready.
      let snippetsPromise = promiseSetupSnippetsMap(tab, test.setup);

      // Start loading about:home and wait for it to complete.
      yield promiseTabLoadEvent(tab, "about:home", "AboutHomeLoadSnippetsSucceeded");

      // This promise should already be resolved since the page is done,
      // but we still want to get the snippets map out of it.
      let snippetsMap = yield snippetsPromise;

      info("Running test");
      yield test.run(snippetsMap);
      info("Cleanup");
      gBrowser.removeCurrentTab();
    }
  }).then(finish, ex => {
    ok(false, "Unexpected Exception: " + ex);
    finish();
  });
}

/**
 * Starts a load in an existing tab and waits for it to finish (via some event).
 *
 * @param aTab
 *        The tab to load into.
 * @param aUrl
 *        The url to load.
 * @param aEvent
 *        The load event type to wait for.  Defaults to "load".
 * @return {Promise} resolved when the event is handled.
 */
function promiseTabLoadEvent(aTab, aURL, aEventType="load")
{
  let deferred = Promise.defer();
  info("Wait tab event: " + aEventType);
  aTab.linkedBrowser.addEventListener(aEventType, function load(event) {
    if (event.originalTarget != aTab.linkedBrowser.contentDocument ||
        event.target.location.href == "about:blank") {
      info("skipping spurious load event");
      return;
    }
    aTab.linkedBrowser.removeEventListener(aEventType, load, true);
    info("Tab event received: " + aEventType);
    deferred.resolve();
  }, true, true);
  aTab.linkedBrowser.loadURI(aURL);
  return deferred.promise;
}

/**
 * Cleans up snippets and ensures that by default we don't try to check for
 * remote snippets since that may cause network bustage or slowness.
 *
 * @param aTab
 *        The tab containing about:home.
 * @param aSetupFn
 *        The setup function to be run.
 * @return {Promise} resolved when the snippets are ready.  Gets the snippets map.
 */
function promiseSetupSnippetsMap(aTab, aSetupFn)
{
  let deferred = Promise.defer();
  info("Waiting for snippets map");
  aTab.linkedBrowser.addEventListener("AboutHomeLoadSnippets", function load(event) {
    aTab.linkedBrowser.removeEventListener("AboutHomeLoadSnippets", load, true);

    let cw = aTab.linkedBrowser.contentWindow.wrappedJSObject;
    // The snippets should already be ready by this point. Here we're
    // just obtaining a reference to the snippets map.
    cw.ensureSnippetsMapThen(function (aSnippetsMap) {
      aSnippetsMap = Cu.waiveXrays(aSnippetsMap);
      info("Got snippets map: " +
           "{ last-update: " + aSnippetsMap.get("snippets-last-update") +
           ", cached-version: " + aSnippetsMap.get("snippets-cached-version") +
           " }");
      // Don't try to update.
      aSnippetsMap.set("snippets-last-update", Date.now());
      aSnippetsMap.set("snippets-cached-version", AboutHomeUtils.snippetsVersion);
      // Clear snippets.
      aSnippetsMap.delete("snippets");
      aSetupFn(aSnippetsMap);
      deferred.resolve(aSnippetsMap);
    });
  }, true, true);
  return deferred.promise;
}

/**
 * Waits for the attributes being set by browser.js.
 *
 * @param aTab
 *        The tab containing about:home.
 * @return {Promise} resolved when the attributes are ready.
 */
function promiseBrowserAttributes(aTab)
{
  let deferred = Promise.defer();

  let docElt = aTab.linkedBrowser.contentDocument.documentElement;
  let observer = new MutationObserver(function (mutations) {
    for (let mutation of mutations) {
      info("Got attribute mutation: " + mutation.attributeName +
                                    " from " + mutation.oldValue);
      // Now we just have to wait for the last attribute.
      if (mutation.attributeName == "searchEngineName") {
        info("Remove attributes observer");
        observer.disconnect();
        // Must be sure to continue after the page mutation observer.
        executeSoon(function() deferred.resolve());
        break;
      }
    }
  });
  info("Add attributes observer");
  observer.observe(docElt, { attributes: true });

  return deferred.promise;
}

/**
 * Retrieves the number of about:home searches recorded for the current day.
 *
 * @param aEngineName
 *        name of the setup search engine.
 *
 * @return {Promise} Returns a promise resolving to the number of searches.
 */
function getNumberOfSearches(aEngineName) {
  let reporter = Components.classes["@mozilla.org/datareporting/service;1"]
                                   .getService()
                                   .wrappedJSObject
                                   .healthReporter;
  ok(reporter, "Health Reporter instance available.");

  return reporter.onInit().then(function onInit() {
    let provider = reporter.getProvider("org.mozilla.searches");
    ok(provider, "Searches provider is available.");

    let m = provider.getMeasurement("counts", 3);
    return m.getValues().then(data => {
      let now = new Date();
      let yday = new Date(now);
      yday.setDate(yday.getDate() - 1);

      // Add the number of searches recorded yesterday to the number of searches
      // recorded today. This makes the test not fail intermittently when it is
      // run at midnight and we accidentally compare the number of searches from
      // different days. Tests are always run with an empty profile so there
      // are no searches from yesterday, normally. Should the test happen to run
      // past midnight we make sure to count them in as well.
      return getNumberOfSearchesByDate(aEngineName, data, now) +
             getNumberOfSearchesByDate(aEngineName, data, yday);
    });
  });
}

function getNumberOfSearchesByDate(aEngineName, aData, aDate) {
  if (aData.days.hasDay(aDate)) {
    let id = Services.search.getEngineByName(aEngineName).identifier;

    let day = aData.days.getDay(aDate);
    let field = id + ".abouthome";

    if (day.has(field)) {
      return day.get(field) || 0;
    }
  }

  return 0; // No records found.
}

function waitForLoad(cb) {
  let browser = gBrowser.selectedBrowser;
  browser.addEventListener("load", function listener() {
    if (browser.currentURI.spec == "about:blank")
      return;
    info("Page loaded: " + browser.currentURI.spec);
    browser.removeEventListener("load", listener, true);

    cb();
  }, true);
}
