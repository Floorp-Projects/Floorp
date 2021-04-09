/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test Google search plugin URLs
 * TODO: This test is a near duplicate of browser_searchEngine_behaviors.js but
 * specific to Google. This is required due to bug 1315953.
 *
 * Note: Although we have tests for codes in
 * toolkit/components/tests/xpcshell/searchconfigs, we also need this test as an
 * integration test to check the search service to selector integration is
 * working correctly (especially the ESR codes).
 */

"use strict";

let searchEngineDetails = [
  {
    alias: "g",
    codes: {
      context: "",
      keyword: "",
      newTab: "",
      submission: "",
    },
    name: "Google",
  },
];

let region = Services.prefs.getCharPref("browser.search.region");
let code = "";
switch (region) {
  case "US":
    if (AppConstants.MOZ_APP_VERSION_DISPLAY.endsWith("esr")) {
      code = "firefox-b-1-e";
    } else {
      code = "firefox-b-1-d";
    }
    break;
  case "DE":
    if (AppConstants.MOZ_APP_VERSION_DISPLAY.endsWith("esr")) {
      code = "firefox-b-e";
    } else {
      code = "firefox-b-d";
    }
    break;
}

if (code) {
  let codes = searchEngineDetails[0].codes;
  codes.context = code;
  codes.newTab = code;
  codes.submission = code;
  codes.keyword = code;
}

function promiseContentSearchReady(browser) {
  return SpecialPowers.spawn(browser, [], async function(args) {
    return new Promise(resolve => {
      SpecialPowers.pushPrefEnv({
        set: [
          [
            "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar",
            false,
          ],
        ],
      });
      if (content.wrappedJSObject.gContentSearchController) {
        let searchController = content.wrappedJSObject.gContentSearchController;
        if (searchController.defaultEngine) {
          resolve();
        }
      }

      content.addEventListener("ContentSearchService", function listener(
        aEvent
      ) {
        if (aEvent.detail.type == "State") {
          content.removeEventListener("ContentSearchService", listener);
          resolve();
        }
      });
    });
  });
}

add_task(async function setup() {
  await Services.search.init();
});

for (let engine of searchEngineDetails) {
  add_task(async function() {
    let previouslySelectedEngine = Services.search.defaultEngine;

    registerCleanupFunction(function() {
      Services.search.defaultEngine = previouslySelectedEngine;
    });

    await testSearchEngine(engine);
  });
}

async function testSearchEngine(engineDetails) {
  let engine = Services.search.getEngineByName(engineDetails.name);
  Assert.ok(engine, `${engineDetails.name} is installed`);

  Services.search.defaultEngine = engine;
  engine.alias = engineDetails.alias;

  // Test search URLs (including purposes).
  let url = engine.getSubmission("foo").uri.spec;
  let urlParams = new URLSearchParams(url.split("?")[1]);
  Assert.equal(urlParams.get("q"), "foo", "Check search URL for 'foo'");

  let engineTests = [
    {
      name: "context menu search",
      code: engineDetails.codes.context,
      run() {
        // Simulate a contextmenu search
        // FIXME: This is a bit "low-level"...
        BrowserSearch._loadSearch(
          "foo",
          false,
          false,
          "contextmenu",
          Services.scriptSecurityManager.getSystemPrincipal()
        );
      },
    },
    {
      name: "keyword search",
      code: engineDetails.codes.keyword,
      run() {
        gURLBar.value = "? foo";
        gURLBar.focus();
        EventUtils.synthesizeKey("KEY_Enter");
      },
    },
    {
      name: "keyword search with alias",
      code: engineDetails.codes.keyword,
      run() {
        gURLBar.value = `${engineDetails.alias} foo`;
        gURLBar.focus();
        EventUtils.synthesizeKey("KEY_Enter");
      },
    },
    {
      name: "search bar search",
      code: engineDetails.codes.submission,
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
      },
    },
    {
      name: "new tab search",
      code: engineDetails.codes.newTab,
      async preTest(tab) {
        let browser = tab.linkedBrowser;
        BrowserTestUtils.loadURI(browser, "about:newtab");
        await BrowserTestUtils.browserLoaded(browser, false, "about:newtab");

        await promiseContentSearchReady(browser);
      },
      async run(tab) {
        await SpecialPowers.spawn(tab.linkedBrowser, [], async function(args) {
          let input = content.document.querySelector("input[id*=search-]");
          input.focus();
          input.value = "foo";
        });
        EventUtils.synthesizeKey("KEY_Enter");
      },
    },
  ];

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  for (let test of engineTests) {
    info(`Running: ${test.name}`);

    if (test.preTest) {
      await test.preTest(tab);
    }

    let promises = [
      BrowserTestUtils.waitForDocLoadAndStopIt(
        "https://www.google.com/search?client=" + test.code + "&q=foo",
        tab
      ),
      BrowserTestUtils.browserStopped(tab.linkedBrowser, null, true),
    ];

    await test.run(tab);

    await Promise.all(promises);

    if (test.postTest) {
      await test.postTest(tab);
    }
  }

  engine.alias = undefined;
  BrowserTestUtils.removeTab(tab);
}
