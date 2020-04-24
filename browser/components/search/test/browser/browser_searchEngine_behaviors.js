/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test search plugin URLs
 */

"use strict";

const SEARCH_ENGINE_DETAILS = [
  {
    alias: "a",
    baseURL:
      "https://www.amazon.com/exec/obidos/external-search/?field-keywords=foo&ie=UTF-8&mode=blended&tag=mozilla-20&sourceid=Mozilla-search",
    codes: {
      context: "",
      keyword: "",
      newTab: "",
      submission: "",
    },
    name: "Amazon.com",
  },
  {
    alias: "b",
    baseURL: "https://www.bing.com/search?{code}pc=MOZI&q=foo",
    codes: {
      context: "form=MOZCON&",
      keyword: "form=MOZLBR&",
      newTab: "form=MOZTSB&",
      submission: "form=MOZSBR&",
    },
    name: "Bing",
  },
  {
    alias: "d",
    baseURL: "https://duckduckgo.com/?{code}q=foo",
    codes: {
      context: "t=ffcm&",
      keyword: "t=ffab&",
      newTab: "t=ffnt&",
      submission: "t=ffsb&",
    },
    name: "DuckDuckGo",
  },
  {
    alias: "e",
    baseURL:
      "https://rover.ebay.com/rover/1/711-53200-19255-0/1?ff3=4&toolid=20004&campid=5338192028&customid=&mpre=https://www.ebay.com/sch/foo",
    codes: {
      context: "",
      keyword: "",
      newTab: "",
      submission: "",
    },
    name: "eBay",
  },
  // {
  // TODO: Google is tested in browser_google_behaviors.js - we can't test it here
  // yet because of bug 1315953.
  //   alias: "g",
  //   baseURL: "https://www.google.com/search?q=foo&ie=utf-8&oe=utf-8",
  //   codes: {
  //     context: "",
  //     keyword: "",
  //     newTab: "",
  //     submission: "",
  //   },
  //   name: "Google",
  // },
];

function promiseContentSearchReady(browser) {
  return SpecialPowers.spawn(browser, [], async function(args) {
    await ContentTaskUtils.waitForCondition(
      () =>
        content.wrappedJSObject.gContentSearchController &&
        content.wrappedJSObject.gContentSearchController.defaultEngine
    );
  });
}

add_task(async function test_setup() {
  await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
});

for (let engine of SEARCH_ENGINE_DETAILS) {
  add_task(async function() {
    let previouslySelectedEngine = await Services.search.getDefault();

    registerCleanupFunction(async function() {
      await Services.search.setDefault(previouslySelectedEngine);
    });

    await testSearchEngine(engine);
  });
}

async function testSearchEngine(engineDetails) {
  let engine = Services.search.getEngineByName(engineDetails.name);
  Assert.ok(engine, `${engineDetails.name} is installed`);

  await Services.search.setDefault(engine);
  engine.alias = engineDetails.alias;

  let base = engineDetails.baseURL;

  // Test search URLs (including purposes).
  let url = engine.getSubmission("foo").uri.spec;
  Assert.equal(
    url,
    base.replace("{code}", engineDetails.codes.submission),
    "Check search URL for 'foo'"
  );
  let sb = BrowserSearch.searchBar;

  let engineTests = [
    {
      name: "context menu search",
      searchURL: base.replace("{code}", engineDetails.codes.context),
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
      searchURL: base.replace("{code}", engineDetails.codes.keyword),
      run() {
        gURLBar.value = "? foo";
        gURLBar.focus();
        EventUtils.synthesizeKey("KEY_Enter");
      },
    },
    {
      name: "keyword search with alias",
      searchURL: base.replace("{code}", engineDetails.codes.keyword),
      run() {
        gURLBar.value = `${engineDetails.alias} foo`;
        gURLBar.focus();
        EventUtils.synthesizeKey("KEY_Enter");
      },
    },
    {
      name: "search bar search",
      searchURL: base.replace("{code}", engineDetails.codes.submission),
      run() {
        sb.focus();
        sb.value = "foo";
        EventUtils.synthesizeKey("KEY_Enter");
      },
    },
    {
      name: "new tab search",
      searchURL: base.replace("{code}", engineDetails.codes.newTab),
      async preTest(tab) {
        let browser = tab.linkedBrowser;
        BrowserTestUtils.loadURI(browser, "about:newtab");
        await BrowserTestUtils.browserLoaded(browser, false, "about:newtab");

        await promiseContentSearchReady(browser);
      },
      async run(tab) {
        await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
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

    let stateChangePromise = promiseStateChangeURI();

    await test.run(tab);

    let receivedURI = await stateChangePromise;

    Assert.equal(receivedURI, test.searchURL);
  }

  engine.alias = undefined;
  sb.value = "";
  BrowserTestUtils.removeTab(tab);
}
