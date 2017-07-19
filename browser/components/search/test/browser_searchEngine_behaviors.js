/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test DuckDuckGo search plugin URLs
 */

"use strict";

const BROWSER_SEARCH_PREF = "browser.search.";


function test() {
  let engine = Services.search.getEngineByName("DuckDuckGo");
  ok(engine, "DuckDuckGo is installed");

  let previouslySelectedEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;
  engine.alias = "d";

  let base = "https://duckduckgo.com/?q=foo";
  let url;

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, base + "&t=ffsb", "Check search URL for 'foo'");

  waitForExplicitFinish();

  var gCurrTest;
  var gTests = [
    {
      name: "context menu search",
      searchURL: base + "&t=ffcm",
      run() {
        // Simulate a contextmenu search
        // FIXME: This is a bit "low-level"...
        BrowserSearch.loadSearch("foo", false, "contextmenu");
      }
    },
    {
      name: "keyword search",
      searchURL: base + "&t=ffab",
      run() {
        gURLBar.value = "? foo";
        gURLBar.focus();
        EventUtils.synthesizeKey("VK_RETURN", {});
      }
    },
    {
      name: "keyword search with alias",
      searchURL: base + "&t=ffab",
      run() {
        gURLBar.value = "d foo";
        gURLBar.focus();
        EventUtils.synthesizeKey("VK_RETURN", {});
      }
    },
    {
      name: "search bar search",
      searchURL: base + "&t=ffsb",
      run() {
        let sb = BrowserSearch.searchBar;
        sb.focus();
        sb.value = "foo";
        registerCleanupFunction(function() {
          sb.value = "";
        });
        EventUtils.synthesizeKey("VK_RETURN", {});
      }
    },
    {
      name: "new tab search",
      searchURL: base + "&t=ffnt",
      run() {
        function doSearch(doc) {
          // Re-add the listener, and perform a search
          gBrowser.addProgressListener(listener);
          let input = doc.querySelector("input[id*=search-]");
          input.focus();
          input.value = "foo";
          EventUtils.synthesizeKey("VK_RETURN", {});
        }

        // load about:newtab, but remove the listener first so it doesn't
        // get in the way
        gBrowser.removeProgressListener(listener);
        gBrowser.loadURI("about:newtab");
        info("Waiting for about:newtab load");
        tab.linkedBrowser.addEventListener("load", function load(loadEvent) {
          if (loadEvent.originalTarget != tab.linkedBrowser.contentDocumentAsCPOW ||
              loadEvent.target.location.href == "about:blank") {
            info("skipping spurious load event");
            return;
          }
          tab.linkedBrowser.removeEventListener("load", load, true);

          // Observe page setup
          let win = gBrowser.contentWindowAsCPOW;
          info("Waiting for newtab search init");
          win.addEventListener("ContentSearchService", function done(contentSearchServiceEvent) {
            info("Got newtab search event " + contentSearchServiceEvent.detail.type);
            if (contentSearchServiceEvent.detail.type == "State") {
              win.removeEventListener("ContentSearchService", done);
              // Let gSearch respond to the event before continuing.
              executeSoon(() => doSearch(win.document));
            }
          });
        }, true);
      }
    }
  ];

  function nextTest() {
    if (gTests.length) {
      gCurrTest = gTests.shift();
      info("Running : " + gCurrTest.name);
      executeSoon(gCurrTest.run);
    } else {
      finish();
    }
  }

  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  let listener = {
    onStateChange: function onStateChange(webProgress, req, flags, status) {
      info("onStateChange");
      // Only care about top-level document starts
      let docStart = Ci.nsIWebProgressListener.STATE_IS_DOCUMENT |
                     Ci.nsIWebProgressListener.STATE_START;
      if (!(flags & docStart) || !webProgress.isTopLevel)
        return;

      if (req.originalURI.spec == "about:blank")
        return;

      info("received document start");

      ok(req instanceof Ci.nsIChannel, "req is a channel");
      is(req.originalURI.spec, gCurrTest.searchURL, "search URL was loaded");
      info("Actual URI: " + req.URI.spec);

      req.cancel(Components.results.NS_ERROR_FAILURE);

      executeSoon(nextTest);
    }
  }

  registerCleanupFunction(function() {
    engine.alias = undefined;
    gBrowser.removeProgressListener(listener);
    gBrowser.removeTab(tab);
    Services.search.currentEngine = previouslySelectedEngine;
  });

  tab.linkedBrowser.addEventListener("load", function() {
    gBrowser.addProgressListener(listener);
    nextTest();
  }, {capture: true, once: true});
}
