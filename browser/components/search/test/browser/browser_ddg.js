/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test DuckDuckGo search plugin URLs
 */

"use strict";

add_task(async function test() {
  await Services.search.init();

  let engine = Services.search.getEngineByName("DuckDuckGo");
  ok(engine, "DuckDuckGo");

  let base = "https://duckduckgo.com/?t={code}&q=foo";
  let url;

  function getUrl(code) {
    return base.replace("{code}", code);
  }

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, getUrl("ffsb"), "Check search URL for 'foo'");
  url = engine.getSubmission("foo", null, "contextmenu").uri.spec;
  is(url, getUrl("ffcm"), "Check context menu search URL for 'foo'");
  url = engine.getSubmission("foo", null, "keyword").uri.spec;
  is(url, getUrl("ffab"), "Check keyword search URL for 'foo'");
  url = engine.getSubmission("foo", null, "searchbar").uri.spec;
  is(url, getUrl("ffsb"), "Check search bar search URL for 'foo'");
  url = engine.getSubmission("foo", null, "homepage").uri.spec;
  is(url, getUrl("ffhp"), "Check homepage search URL for 'foo'");
  url = engine.getSubmission("foo", null, "newtab").uri.spec;
  is(url, getUrl("ffnt"), "Check newtab search URL for 'foo'");

  // Check search suggestion URL.
  url = engine.getSubmission("foo", "application/x-suggestions+json").uri.spec;
  is(
    url,
    "https://ac.duckduckgo.com/ac/?q=foo&type=list",
    "Check search suggestion URL for 'foo'"
  );

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
            {
              name: "q",
              value: "{searchTerms}",
              purpose: undefined,
            },
          ],
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
