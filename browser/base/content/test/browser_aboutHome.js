/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/commonjs/sdk/core/promise.js");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AboutHomeUtils",
  "resource:///modules/AboutHomeUtils.jsm");

registerCleanupFunction(function() {
  // Ensure we don't pollute prefs for next tests.
  try {
    Services.prefs.clearUserPref("network.cookies.cookieBehavior");
  } catch (ex) {}
  try {
    Services.prefs.clearUserPref("network.cookie.lifetimePolicy");
  } catch (ex) {}
});

let gTests = [

{
  desc: "Check that clearing cookies does not clear storage",
  setup: function ()
  {
    Cc["@mozilla.org/dom/storagemanager;1"]
      .getService(Ci.nsIObserver)
      .observe(null, "cookie-changed", "cleared");
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
  desc: "Check that performing a search fires a search event.",
  setup: function () { },
  run: function () {
    let deferred = Promise.defer();
    let doc = gBrowser.contentDocument;

    doc.addEventListener("AboutHomeSearchEvent", function onSearch(e) {
      is(e.detail, doc.documentElement.getAttribute("searchEngineName"), "Detail is search engine name");

      gBrowser.stop();
      deferred.resolve();
    }, true, true);

    doc.getElementById("searchText").value = "it works";
    doc.getElementById("searchSubmit").click();
    return deferred.promise;
  }
},

{
  desc: "Check that performing a search records to Firefox Health Report.",
  setup: function () { },
  run: function () {
    try {
      let cm = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
      cm.getCategoryEntry("healthreport-js-provider", "SearchesProvider");
    } catch (ex) {
      // Health Report disabled, or no SearchesProvider.
      return Promise.resolve();
    }

    let deferred = Promise.defer();
    let doc = gBrowser.contentDocument;

    // We rely on the listener in browser.js being installed and fired before
    // this one. If this ever changes, we should add an executeSoon() or similar.
    doc.addEventListener("AboutHomeSearchEvent", function onSearch(e) {
      executeSoon(gBrowser.stop.bind(gBrowser));
      let reporter = Components.classes["@mozilla.org/datareporting/service;1"]
                                       .getService()
                                       .wrappedJSObject
                                       .healthReporter;
      ok(reporter, "Health Reporter instance available.");

      reporter.onInit().then(function onInit() {
        let provider = reporter.getProvider("org.mozilla.searches");
        ok(provider, "Searches provider is available.");

        let engineName = doc.documentElement.getAttribute("searchEngineName").toLowerCase();

        let m = provider.getMeasurement("counts", 1);
        m.getValues().then(function onValues(data) {
          let now = new Date();
          ok(data.days.hasDay(now), "Have data for today.");

          let day = data.days.getDay(now);
          let field = engineName + ".abouthome";
          ok(day.has(field), "Have data for about home on this engine.");

          // Note the search from the previous test.
          is(day.get(field), 2, "Have searches recorded.");

          deferred.resolve();
        });

      });
    }, true, true);

    doc.getElementById("searchText").value = "a search";
    doc.getElementById("searchSubmit").click();
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

];

function test()
{
  waitForExplicitFinish();

  Task.spawn(function () {
    for (let test of gTests) {
      info(test.desc);

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
