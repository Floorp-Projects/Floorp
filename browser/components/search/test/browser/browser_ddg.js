/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test DuckDuckGo search plugin URLs
 */

"use strict";

const BROWSER_SEARCH_PREF = "browser.search.";

add_task(async function test() {
  await Services.search.init();

  let engine = Services.search.getEngineByName("DuckDuckGo");
  ok(engine, "DuckDuckGo");

  let base = "https://duckduckgo.com/?q=foo";
  let url;

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, base + "&t=ffsb", "Check search URL for 'foo'");
  url = engine.getSubmission("foo", null, "contextmenu").uri.spec;
  is(url, base + "&t=ffcm", "Check context menu search URL for 'foo'");
  url = engine.getSubmission("foo", null, "keyword").uri.spec;
  is(url, base + "&t=ffab", "Check keyword search URL for 'foo'");
  url = engine.getSubmission("foo", null, "searchbar").uri.spec;
  is(url, base + "&t=ffsb", "Check search bar search URL for 'foo'");
  url = engine.getSubmission("foo", null, "homepage").uri.spec;
  is(url, base + "&t=ffhp", "Check homepage search URL for 'foo'");
  url = engine.getSubmission("foo", null, "newtab").uri.spec;
  is(url, base + "&t=ffnt", "Check newtab search URL for 'foo'");

  // Check search suggestion URL.
  url = engine.getSubmission("foo", "application/x-suggestions+json").uri.spec;
  is(url, "https://ac.duckduckgo.com/ac/?q=foo&type=list", "Check search suggestion URL for 'foo'");

  // Check all other engine properties.
  const EXPECTED_ENGINE = {
    name: "DuckDuckGo",
    alias: null,
    description: "Search DuckDuckGo",
    searchForm: "https://duckduckgo.com/?q=",
    hidden: false,
    wrappedJSObject: {
      queryCharset: "UTF-8",
      _urls: [
        {
          type: "text/html",
          method: "GET",
          template: "https://duckduckgo.com/",
          params: [
            {
              name: "q",
              value: "{searchTerms}",
              purpose: undefined,
            },
            {
              name: "t",
              value: "ffcm",
              purpose: "contextmenu",
            },
            {
              name: "t",
              value: "ffab",
              purpose: "keyword",
            },
            {
              name: "t",
              value: "ffsb",
              purpose: "searchbar",
            },
            {
              name: "t",
              value: "ffhp",
              purpose: "homepage",
            },
            {
              name: "t",
              value: "ffnt",
              purpose: "newtab",
            },
          ],
          mozparams: {},
        },
        {
          type: "application/x-suggestions+json",
          method: "GET",
          template: "https://ac.duckduckgo.com/ac/",
          params: [
            {
              name: "q",
              value: "{searchTerms}",
              purpose: undefined,
            },
            {
              name: "type",
              value: "list",
              purpose: undefined,
            },
          ],
        },
      ],
    },
  };

  isSubObjectOf(EXPECTED_ENGINE, engine, "DuckDuckGo");
});
