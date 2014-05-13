/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test Google search plugin URLs
 */

"use strict";

const BROWSER_SEARCH_PREF      = "browser.search.";

const MOZ_PARAM_LOCALE         = /\{moz:locale\}/g;
const MOZ_PARAM_DIST_ID        = /\{moz:distributionID\}/g;
const MOZ_PARAM_OFFICIAL       = /\{moz:official\}/g;

let runtime = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
// Custom search parameters
const MOZ_OFFICIAL = runtime.isOfficialBranding ? "official" : "unofficial";

var google_client;
switch (runtime.defaultUpdateChannel) {
case "beta":
  google_client = "firefox-beta";
  break;
case "aurora":
  google_client = "firefox-aurora";
  break;
case "nightly":
  google_client = "firefox-nightly";
  break;
default:
  google_client = "firefox-a";
  break;
}

const GOOGLE_CLIENT = google_client;
const MOZ_DISTRIBUTION_ID = runtime.distributionID;

function test() {
  let engine = Services.search.getEngineByName("Google");
  ok(engine, "Google is installed");

  is(Services.search.defaultEngine, engine, "Check that Google is the default search engine");

  let distributionID;
  try {
    distributionID = Services.prefs.getCharPref(BROWSER_SEARCH_PREF + "distributionID");
  } catch (ex) {
    distributionID = MOZ_DISTRIBUTION_ID;
  }

  let base = "https://www.google.com/search?q=foo&ie=utf-8&oe=utf-8&aq=t&rls={moz:distributionID}:{moz:locale}:{moz:official}&client=" + GOOGLE_CLIENT;
  base = base.replace(MOZ_PARAM_LOCALE, getLocale());
  base = base.replace(MOZ_PARAM_DIST_ID, distributionID);
  base = base.replace(MOZ_PARAM_OFFICIAL, MOZ_OFFICIAL);

  let url;

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, base, "Check search URL for 'foo'");

  waitForExplicitFinish();

  var gCurrTest;
  var gTests = [
    {
      name: "context menu search",
      searchURL: base + "&channel=rcs",
      run: function () {
        // Simulate a contextmenu search
        // FIXME: This is a bit "low-level"...
        BrowserSearch.loadSearch("foo", false, "contextmenu");
      }
    },
    {
      name: "keyword search",
      searchURL: base + "&channel=fflb",
      run: function () {
        gURLBar.value = "? foo";
        gURLBar.focus();
        EventUtils.synthesizeKey("VK_RETURN", {});
      }
    },
    {
      name: "search bar search",
      searchURL: base + "&channel=sb",
      run: function () {
        let sb = BrowserSearch.searchBar;
        sb.focus();
        sb.value = "foo";
        registerCleanupFunction(function () {
          sb.value = "";
        });
        EventUtils.synthesizeKey("VK_RETURN", {});
      }
    },
    {
      name: "new tab search",
      searchURL: base + "&channel=nts",
      run: function () {
        function doSearch(doc) {
          // Re-add the listener, and perform a search
          gBrowser.addProgressListener(listener);
          doc.getElementById("newtab-search-text").value = "foo";
          doc.getElementById("newtab-search-submit").click();
        }

        // load about:newtab, but remove the listener first so it doesn't
        // get in the way
        gBrowser.removeProgressListener(listener);
        gBrowser.loadURI("about:newtab");
        info("Waiting for about:newtab load");
        tab.linkedBrowser.addEventListener("load", function load(event) {
          if (event.originalTarget != tab.linkedBrowser.contentDocument ||
              event.target.location.href == "about:blank") {
            info("skipping spurious load event");
            return;
          }
          tab.linkedBrowser.removeEventListener("load", load, true);

          // Observe page setup
          let win = gBrowser.contentWindow;
          if (win.gSearch.currentEngineName ==
              Services.search.currentEngine.name) {
            doSearch(win.document);
          }
          else {
            info("Waiting for newtab search init");
            win.addEventListener("ContentSearchService", function done(event) {
              info("Got newtab search event " + event.detail.type);
              if (event.detail.type == "State") {
                win.removeEventListener("ContentSearchService", done);
                // Let gSearch respond to the event before continuing.
                executeSoon(() => doSearch(win.document));
              }
            });
          }
        }, true);
      }
    },
    {
      name: "home page search",
      searchURL: base + "&channel=np&source=hp",
      run: function () {
        // Bug 992270: Ignore uncaught about:home exceptions (related to snippets from IndexedDB)
        ignoreAllUncaughtExceptions(true);

        // load about:home, but remove the listener first so it doesn't
        // get in the way
        gBrowser.removeProgressListener(listener);
        gBrowser.loadURI("about:home");
        info("Waiting for about:home load");
        tab.linkedBrowser.addEventListener("load", function load(event) {
          if (event.originalTarget != tab.linkedBrowser.contentDocument ||
              event.target.location.href == "about:blank") {
            info("skipping spurious load event");
            return;
          }
          tab.linkedBrowser.removeEventListener("load", load, true);

          // Observe page setup
          let doc = gBrowser.contentDocument;
          let mutationObserver = new MutationObserver(function (mutations) {
            for (let mutation of mutations) {
              if (mutation.attributeName == "searchEngineName") {
                // Re-add the listener, and perform a search
                gBrowser.addProgressListener(listener);
                doc.getElementById("searchText").value = "foo";
                doc.getElementById("searchSubmit").click();
              }
            }
          });
          mutationObserver.observe(doc.documentElement, { attributes: true });
        }, true);
      }
    }
  ];

  function nextTest() {
    // Make sure we listen again for uncaught exceptions in the next test or cleanup.
    ignoreAllUncaughtExceptions(false);

    if (gTests.length) {
      gCurrTest = gTests.shift();
      info("Running : " + gCurrTest.name);
      executeSoon(gCurrTest.run);
    } else {
      finish();
    }
  }

  let tab = gBrowser.selectedTab = gBrowser.addTab();

  let listener = {
    onStateChange: function onStateChange(webProgress, req, flags, status) {
      info("onStateChange");
      // Only care about top-level document starts
      let docStart = Ci.nsIWebProgressListener.STATE_IS_DOCUMENT |
                     Ci.nsIWebProgressListener.STATE_START;
      if (!(flags & docStart) || !webProgress.isTopLevel)
        return;

      info("received document start");

      ok(req instanceof Ci.nsIChannel, "req is a channel");
      is(req.originalURI.spec, gCurrTest.searchURL, "search URL was loaded");
      info("Actual URI: " + req.URI.spec);

      req.cancel(Components.results.NS_ERROR_FAILURE);

      executeSoon(nextTest);
    }
  }

  registerCleanupFunction(function () {
    gBrowser.removeProgressListener(listener);
    gBrowser.removeTab(tab);
  });

  tab.linkedBrowser.addEventListener("load", function load() {
    tab.linkedBrowser.removeEventListener("load", load, true);
    gBrowser.addProgressListener(listener);
    nextTest();
  }, true);
}
