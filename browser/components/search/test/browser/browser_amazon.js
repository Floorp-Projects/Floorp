/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test Amazon search plugin URLs
 */

"use strict";

const BROWSER_SEARCH_PREF = "browser.search.";

add_task(async function test() {
  await Services.search.init();

  let engine = Services.search.getEngineByName("Amazon.com");
  ok(engine, "Amazon.com");

  let base = "https://www.amazon.com/exec/obidos/external-search/?field-keywords=foo&ie=UTF-8&mode=blended&tag=mozilla-20&sourceid=Mozilla-search";
  let url;

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, base, "Check search URL for 'foo'");

  // Check search suggestion URL.
  url = engine.getSubmission("foo", "application/x-suggestions+json").uri.spec;
  is(url, "https://completion.amazon.com/search/complete?q=foo&search-alias=aps&mkt=1", "Check search suggestion URL for 'foo'");

  // Check all other engine properties.
  const EXPECTED_ENGINE = {
    name: "Amazon.com",
    alias: null,
    hidden: false,
    wrappedJSObject: {
      queryCharset: "UTF-8",
      _urls: [
        {
          type: "text/html",
          method: "GET",
          template: "https://www.amazon.com/exec/obidos/external-search/",
          params: [
            {
              name: "field-keywords",
              value: "{searchTerms}",
              purpose: undefined,
            },
            {
              name: "ie",
              value: "{inputEncoding}",
              purpose: undefined,
            },
            {
              name: "mode",
              value: "blended",
              purpose: undefined,
            },
            {
              name: "tag",
              value: "mozilla-20",
              purpose: undefined,
            },
            {
              name: "sourceid",
              value: "Mozilla-search",
              purpose: undefined,
            },
          ],
          mozparams: {},
        },
        {
          type: "application/x-suggestions+json",
          method: "GET",
          template: "https://completion.amazon.com/search/complete?q={searchTerms}&search-alias=aps&mkt=1",
        },
      ],
    },
  };

  isSubObjectOf(EXPECTED_ENGINE, engine, "Amazon");
});
