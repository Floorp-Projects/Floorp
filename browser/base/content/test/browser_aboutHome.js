/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/commonjs/sdk/core/promise.js");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AboutHomeUtils",
  "resource:///modules/AboutHomeUtils.jsm");

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

{
  desc: "Check that performing a search fires a search event and records to " +
        "Firefox Health Report.",
  setup: function () { },
  run: function () {
    try {
      let cm = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
      cm.getCategoryEntry("healthreport-js-provider-default", "SearchesProvider");
    } catch (ex) {
      // Health Report disabled, or no SearchesProvider.
      return Promise.resolve();
    }

    let numSearchesBefore = 0;
    let deferred = Promise.defer();
    let doc = gBrowser.contentDocument;
    let engineName = doc.documentElement.getAttribute("searchEngineName");

    // We rely on the listener in browser.js being installed and fired before
    // this one. If this ever changes, we should add an executeSoon() or similar.
    doc.addEventListener("AboutHomeSearchEvent", function onSearch(e) {
      is(e.detail, engineName, "Detail is search engine name");

      getNumberOfSearches(engineName).then(num => {
        is(num, numSearchesBefore + 1, "One more search recorded.");
        deferred.resolve();
      });
    }, true, true);

    // Get the current number of recorded searches.
    getNumberOfSearches(engineName).then(num => {
      numSearchesBefore = num;

      info("Perform a search.");
      doc.getElementById("searchText").value = "a search";
      doc.getElementById("searchSubmit").click();
      gBrowser.stop();
    });

    return deferred.promise;
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

      Services.search.defaultEngine = engine;

      registerCleanupFunction(function() {
        Services.search.removeEngine(engine);
        Services.search.defaultEngine = currEngine;
      });


      // Ready to execute the tests!
      let needle = "Search for something awesome.";
      let document = gBrowser.selectedTab.linkedBrowser.contentDocument;
      let searchText = document.getElementById("searchText");

      waitForLoad(function() {
        let loadedText = gBrowser.contentDocument.body.textContent;
        ok(loadedText, "search page loaded");
        is(loadedText, "searchterms=" + escape(needle.replace(/\s/g, "+")),
           "Search text should arrive correctly");
        deferred.resolve();
      });

      searchText.value = needle;
      searchText.focus();
      EventUtils.synthesizeKey("VK_RETURN", {});
    };
    Services.obs.addObserver(searchObserver, "browser-search-engine-modified", false);
    registerCleanupFunction(function () {
      Services.obs.removeObserver(searchObserver, "browser-search-engine-modified");
    });
    Services.search.addEngine("http://test:80/browser/browser/base/content/test/POSTSearchEngine.xml",
                              Ci.nsISearchEngine.DATA_XML, null, false);
    return deferred.promise;
  }
}

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

      let tab = yield promiseNewTabLoadEvent("about:home", "DOMContentLoaded");

      // Must wait for both the snippets map and the browser attributes, since
      // can't guess the order they will happen.
      // So, start listening now, but verify the promise is fulfilled only
      // after the snippets map setup.
      let promise = promiseBrowserAttributes(tab);
      // Prepare the snippets map with default values, then run the test setup.
      let snippetsMap = yield promiseSetupSnippetsMap(tab, test.setup);
      // Ensure browser has set attributes already, or wait for them.
      yield promise;
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
 * Creates a new tab and waits for a load event.
 *
 * @param aUrl
 *        The url to load in a new tab.
 * @param aEvent
 *        The load event type to wait for.  Defaults to "load".
 * @return {Promise} resolved when the event is handled.  Gets the new tab.
 */
function promiseNewTabLoadEvent(aUrl, aEventType="load")
{
  let deferred = Promise.defer();
  let tab = gBrowser.selectedTab = gBrowser.addTab(aUrl);
  info("Wait tab event: " + aEventType);
  tab.linkedBrowser.addEventListener(aEventType, function load(event) {
    if (event.originalTarget != tab.linkedBrowser.contentDocument ||
        event.target.location.href == "about:blank") {
      info("skipping spurious load event");
      return;
    }
    tab.linkedBrowser.removeEventListener(aEventType, load, true);
    info("Tab event received: " + aEventType);
    deferred.resolve(tab);
  }, true);
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
  let cw = aTab.linkedBrowser.contentWindow.wrappedJSObject;
  info("Waiting for snippets map");
  cw.ensureSnippetsMapThen(function (aSnippetsMap) {
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
    // Must be sure to continue after the page snippets map setup.
    executeSoon(function() deferred.resolve(aSnippetsMap));
  });
  return deferred.promise;
}

/**
 * Waits for the attributes being set by browser.js and overwrites snippetsURL
 * to ensure we won't try to hit the network and we can force xhr to throw.
 *
 * @param aTab
 *        The tab containing about:home.
 * @return {Promise} resolved when the attributes are ready.
 */
function promiseBrowserAttributes(aTab)
{
  let deferred = Promise.defer();

  let docElt = aTab.linkedBrowser.contentDocument.documentElement;
  //docElt.setAttribute("snippetsURL", "nonexistent://test");
  let observer = new MutationObserver(function (mutations) {
    for (let mutation of mutations) {
      info("Got attribute mutation: " + mutation.attributeName +
                                    " from " + mutation.oldValue); 
      if (mutation.attributeName == "snippetsURL" &&
          docElt.getAttribute("snippetsURL") != "nonexistent://test") {
        docElt.setAttribute("snippetsURL", "nonexistent://test");
      }

      // Now we just have to wait for the last attribute.
      if (mutation.attributeName == "searchEngineURL") {
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

    let m = provider.getMeasurement("counts", 2);
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
