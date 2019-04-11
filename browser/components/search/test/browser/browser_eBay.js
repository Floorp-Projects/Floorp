/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test eBay search plugin URLs
 */

"use strict";

const BROWSER_SEARCH_PREF = "browser.search.";

add_task(async function test() {
  await Services.search.init();

  let engine = Services.search.getEngineByName("eBay");
  ok(engine, "eBay");

  let base = "https://rover.ebay.com/rover/1/711-53200-19255-0/1?ff3=4&toolid=20004&campid=5338192028&customid=&mpre=https://www.ebay.com/sch/foo";
  let url;

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, base, "Check search URL for 'foo'");

  // Check all other engine properties.
  const EXPECTED_ENGINE = {
    name: "eBay",
    alias: null,
    description: "eBay - Online auctions",
    searchForm: "https://www.ebay.com/",
    hidden: false,
    wrappedJSObject: {
      _urls: [
        {
          type: "text/html",
          method: "GET",
          template: "https://rover.ebay.com/rover/1/711-53200-19255-0/1",
          params: [
            {
              name: "ff3",
              value: "4",
              purpose: undefined,
            },
            {
              name: "toolid",
              value: "20004",
              purpose: undefined,
            },
            {
              name: "campid",
              value: "5338192028",
              purpose: undefined,
            },
            {
              name: "customid",
              value: "",
              purpose: undefined,
            },
            {
              name: "mpre",
              value: "https://www.ebay.com/sch/{searchTerms}",
              purpose: undefined,
            },
          ],
          mozparams: {},
        },
      ],
    },
  };

  isSubObjectOf(EXPECTED_ENGINE, engine, "eBay");
});
