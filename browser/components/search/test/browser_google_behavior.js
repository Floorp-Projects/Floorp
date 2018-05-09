/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test Google search plugin URLs
 * TODO: This test is a near duplicate of browser_searchEngine_behaviors.js but
 * specific to Google. This is required due to bug 1315953.
 */

"use strict";

let searchEngineDetails = [{
  alias: "g",
  baseURL: "https://www.google.com/search?q=foo&ie=utf-8&oe=utf-8",
  codes: {
    context: "",
    keyword: "",
    newTab: "",
    submission: "",
  },
  name: "Google",
}];

let countryCode = Services.prefs.getCharPref("browser.search.countryCode");
let code = "";
switch (countryCode) {
  case "US":
    code = "firefox-b-1";
    break;
  case "DE":
    code = "firefox-b";
    break;
}

if (code) {
  let codes = searchEngineDetails[0].codes;
  let suffix = `&client=${code}`;
  codes.context = suffix;
  codes.newTab = suffix;
  codes.submission = suffix;
  codes.keyword = `${suffix}-ab`;
}

function promiseContentSearchReady(browser) {
  return ContentTask.spawn(browser, {}, async function(args) {
    return new Promise(resolve => {
      if (content.wrappedJSObject.gContentSearchController) {
        let searchController = content.wrappedJSObject.gContentSearchController;
        if (searchController.defaultEngine) {
          resolve();
        }
      }

      content.addEventListener("ContentSearchService", function listener(aEvent) {
        if (aEvent.detail.type == "State") {
          content.removeEventListener("ContentSearchService", listener);
          resolve();
        }
      });
    });
  });
}

for (let engine of searchEngineDetails) {
  add_task(async function() {
    let previouslySelectedEngine = Services.search.currentEngine;

    registerCleanupFunction(function() {
      Services.search.currentEngine = previouslySelectedEngine;
    });

    await testSearchEngine(engine);
  });
}

async function testSearchEngine(engineDetails) {
  let engine = Services.search.getEngineByName(engineDetails.name);
  Assert.ok(engine, `${engineDetails.name} is installed`);

  Services.search.currentEngine = engine;
  engine.alias = engineDetails.alias;

  let base = engineDetails.baseURL;

  // Test search URLs (including purposes).
  let url = engine.getSubmission("foo").uri.spec;
  Assert.equal(url, base + engineDetails.codes.submission, "Check search URL for 'foo'");

  let engineTests = [
    {
      name: "context menu search",
      searchURL: base + engineDetails.codes.context,
      run() {
        // Simulate a contextmenu search
        // FIXME: This is a bit "low-level"...
        BrowserSearch.loadSearch("foo", false, "contextmenu");
      }
    },
    {
      name: "keyword search",
      searchURL: base + engineDetails.codes.keyword,
      run() {
        gURLBar.value = "? foo";
        gURLBar.focus();
        EventUtils.synthesizeKey("KEY_Enter");
      }
    },
    {
      name: "keyword search with alias",
      searchURL: base + engineDetails.codes.keyword,
      run() {
        gURLBar.value = `${engineDetails.alias} foo`;
        gURLBar.focus();
        EventUtils.synthesizeKey("KEY_Enter");
      }
    },
    {
      name: "search bar search",
      searchURL: base + engineDetails.codes.submission,
      async preTest() {
        await gCUITestUtils.addSearchBar();
      },
      run() {
        let sb = BrowserSearch.searchBar;
        sb.focus();
        sb.value = "foo";
        EventUtils.synthesizeKey("KEY_Enter");
      },
      postTest() {
        BrowserSearch.searchBar.value = "";
        gCUITestUtils.removeSearchBar();
      }
    },
    {
      name: "new tab search",
      searchURL: base + engineDetails.codes.newTab,
      async preTest(tab) {
        let browser = tab.linkedBrowser;
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
        EventUtils.synthesizeKey("KEY_Enter");
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

    if (test.postTest) {
      await test.postTest(tab);
    }
  }

  engine.alias = undefined;
  BrowserTestUtils.removeTab(tab);
}
