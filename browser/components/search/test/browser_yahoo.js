/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test Yahoo search plugin URLs
 */

"use strict";

const BROWSER_SEARCH_PREF = "browser.search.";

function test() {
  let engine = Services.search.getEngineByName("Yahoo");
  ok(engine, "Yahoo");

  let base = "https://search.yahoo.com/yhs/search?p=foo&ei=UTF-8&hspart=mozilla";
  let url;

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, base + "&hsimp=yhs-001", "Check search URL for 'foo'");
  url = engine.getSubmission("foo", null, "searchbar").uri.spec;
  is(url, base + "&hsimp=yhs-001", "Check search bar search URL for 'foo'");
  url = engine.getSubmission("foo", null, "keyword").uri.spec;
  is(url, base + "&hsimp=yhs-002", "Check keyword search URL for 'foo'");
  url = engine.getSubmission("foo", null, "homepage").uri.spec;
  is(url, base + "&hsimp=yhs-003", "Check homepage search URL for 'foo'");
  url = engine.getSubmission("foo", null, "newtab").uri.spec;
  is(url, base + "&hsimp=yhs-004", "Check newtab search URL for 'foo'");
  url = engine.getSubmission("foo", null, "contextmenu").uri.spec;
  is(url, base + "&hsimp=yhs-005", "Check context menu search URL for 'foo'");
  url = engine.getSubmission("foo", null, "system").uri.spec;
  is(url, base + "&hsimp=yhs-007", "Check system search URL for 'foo'");
  url = engine.getSubmission("foo", null, "invalid").uri.spec;
  is(url, base + "&hsimp=yhs-001", "Check invalid URL for 'foo'");

  // Check search suggestion URL.
  url = engine.getSubmission("foo", "application/x-suggestions+json").uri.spec;
  is(url, "https://search.yahoo.com/sugg/ff?output=fxjson&appid=ffd&command=foo", "Check search suggestion URL for 'foo'");

  // Check all other engine properties.
  const EXPECTED_ENGINE = {
    name: "Yahoo",
    alias: null,
    description: "Yahoo Search",
    searchForm: "https://search.yahoo.com/yhs/search?p=&ei=UTF-8&hspart=mozilla&hsimp=yhs-001",
    hidden: false,
    wrappedJSObject: {
      queryCharset: "UTF-8",
      "_iconURL": "resource://search-plugins/images/yahoo.ico",
      _urls : [
        {
          type: "application/x-suggestions+json",
          method: "GET",
          template: "https://search.yahoo.com/sugg/ff",
          params: [
            {
              name: "output",
              value: "fxjson",
              purpose: undefined,
            },
            {
              name: "appid",
              value: "ffd",
              purpose: undefined,
            },
            {
              name: "command",
              value: "{searchTerms}",
              purpose: undefined,
            },
          ],
        },
        {
          type: "text/html",
          method: "GET",
          template: "https://search.yahoo.com/yhs/search",
          params: [
            {
              name: "p",
              value: "{searchTerms}",
              purpose: undefined,
            },
            {
              name: "ei",
              value: "UTF-8",
              purpose: undefined,
            },
            {
              name: "hspart",
              value: "mozilla",
              purpose: undefined,
            },
            {
              name: "hsimp",
              value: "yhs-001",
              purpose: "searchbar",
            },
            {
              name: "hsimp",
              value: "yhs-002",
              purpose: "keyword",
            },
            {
              name: "hsimp",
              value: "yhs-003",
              purpose: "homepage",
            },
            {
              name: "hsimp",
              value: "yhs-004",
              purpose: "newtab",
            },
            {
              name: "hsimp",
              value: "yhs-005",
              purpose: "contextmenu",
            },
            {
              name: "hsimp",
              value: "yhs-007",
              purpose: "system",
            },
          ],
          mozparams: {},
        },
      ],
    },
  };

  isSubObjectOf(EXPECTED_ENGINE, engine, "Yahoo");
}
