/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test DuckDuckGo search plugin URLs
 */

"use strict";

function promiseStateChangeURI() {
  return new Promise(resolve => {
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

        gBrowser.removeProgressListener(listener);

        info("received document start");

        Assert.ok(req instanceof Ci.nsIChannel, "req is a channel");

        req.cancel(Components.results.NS_ERROR_FAILURE);

        executeSoon(() => {
          resolve(req.originalURI.spec);
        });
      }
    }

    gBrowser.addProgressListener(listener);
  });
}

function promiseContentSearchReady(browser) {
  return ContentTask.spawn(browser, {}, async function(args) {
    return new Promise(resolve => {
      content.addEventListener("ContentSearchService", function listener(aEvent) {
        if (aEvent.detail.type == "State") {
          content.removeEventListener("ContentSearchService", listener);
          resolve();
        }
      });
    });
  });
}

add_task(async function() {
  let previouslySelectedEngine = Services.search.currentEngine;

  registerCleanupFunction(function() {
    Services.search.currentEngine = previouslySelectedEngine;
  });

  await testSearchEngine();
});

async function testSearchEngine() {
  let engine = Services.search.getEngineByName("DuckDuckGo");
  ok(engine, "DuckDuckGo is installed");

  Services.search.currentEngine = engine;
  engine.alias = "d";

  let base = "https://duckduckgo.com/?q=foo";

  // Test search URLs (including purposes).
  let url = engine.getSubmission("foo").uri.spec;
  Assert.equal(url, base + "&t=ffsb", "Check search URL for 'foo'");

  let engineTests = [
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
      async preTest(tab) {
        let browser = tab.linkedBrowser
        await BrowserTestUtils.loadURI(browser, "about:newtab");
        await BrowserTestUtils.browserLoaded(browser);

        await promiseContentSearchReady(browser);
      },
      async run(tab) {
        await ContentTask.spawn(tab.linkedBrowser, {}, async function(args) {
          let input = content.document.querySelector("input[id*=search-]");
          input.focus();
          input.value = "foo";
        });
        EventUtils.synthesizeKey("VK_RETURN", {});
      }
    }
  ];

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  for (let test of engineTests) {
    info(`Running: ${test.name}`);

    if (test.preTest) {
      await test.preTest(tab);
    }

    let stateChangePromise = promiseStateChangeURI();

    await test.run(tab);

    let receivedURI = await stateChangePromise;

    Assert.equal(receivedURI, test.searchURL);
  }

  engine.alias = undefined;
  await BrowserTestUtils.removeTab(tab);
}
