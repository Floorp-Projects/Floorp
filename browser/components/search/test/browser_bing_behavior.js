/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test Bing search plugin URLs
 */

"use strict";

const BROWSER_SEARCH_PREF = "browser.search.";


function test() {
  let engine = Services.search.getEngineByName("Bing");
  ok(engine, "Bing is installed");

  let previouslySelectedEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;

  let base = "http://www.bing.com/search?q=foo&pc=MOZI";
  let url;

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, base, "Check search URL for 'foo'");

  waitForExplicitFinish();

  var gCurrTest;
  var gTests = [
    {
      name: "context menu search",
      searchURL: base + "&form=MOZCON",
      run: function () {
        // Simulate a contextmenu search
        // FIXME: This is a bit "low-level"...
        BrowserSearch.loadSearch("foo", false, "contextmenu");
      }
    },
    {
      name: "keyword search",
      searchURL: base + "&form=MOZLBR",
      run: function () {
        gURLBar.value = "? foo";
        gURLBar.focus();
        EventUtils.synthesizeKey("VK_RETURN", {});
      }
    },
    {
      name: "search bar search",
      searchURL: base + "&form=MOZSBR",
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
      searchURL: base + "&form=MOZTSB",
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
      searchURL: base + "&form=MOZSPG",
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
    Services.search.currentEngine = previouslySelectedEngine;
  });

  tab.linkedBrowser.addEventListener("load", function load() {
    tab.linkedBrowser.removeEventListener("load", load, true);
    gBrowser.addProgressListener(listener);
    nextTest();
  }, true);
}
