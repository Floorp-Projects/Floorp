/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test Bing search plugin URLs
 */

"use strict";

const BROWSER_SEARCH_PREF = "browser.search.";

add_task(async function test() {
  await Services.search.init();

  let engine = Services.search.getEngineByName("Bing");
  ok(engine, "Bing");

  let base = "https://www.bing.com/search?q=foo&pc=MOZI";
  let url;

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, base + "&form=MOZSBR", "Check search URL for 'foo'");
  url = engine.getSubmission("foo", null, "contextmenu").uri.spec;
  is(url, base + "&form=MOZCON", "Check context menu search URL for 'foo'");
  url = engine.getSubmission("foo", null, "keyword").uri.spec;
  is(url, base + "&form=MOZLBR", "Check keyword search URL for 'foo'");
  url = engine.getSubmission("foo", null, "searchbar").uri.spec;
  is(url, base + "&form=MOZSBR", "Check search bar search URL for 'foo'");
  url = engine.getSubmission("foo", null, "homepage").uri.spec;
  is(url, base + "&form=MOZSPG", "Check homepage search URL for 'foo'");
  url = engine.getSubmission("foo", null, "newtab").uri.spec;
  is(url, base + "&form=MOZTSB", "Check newtab search URL for 'foo'");

  // Check search suggestion URL.
  url = engine.getSubmission("foo", "application/x-suggestions+json").uri.spec;
  is(url, "https://www.bing.com/osjson.aspx?query=foo&form=OSDJAS&language=" + getLocale(), "Check search suggestion URL for 'foo'");

  // Check all other engine properties.
  const EXPECTED_ENGINE = {
    name: "Bing",
    alias: null,
    description: "Bing. Search by Microsoft.",
    searchForm: "https://www.bing.com/search?q=&pc=MOZI",
    hidden: false,
    wrappedJSObject: {
      queryCharset: "UTF-8",
      _urls: [
        {
          type: "text/html",
          method: "GET",
          template: "https://www.bing.com/search",
          params: [
            {
              name: "q",
              value: "{searchTerms}",
              purpose: undefined,
            },
            {
              name: "pc",
              value: "MOZI",
              purpose: undefined,
            },
            {
              name: "form",
              value: "MOZCON",
              purpose: "contextmenu",
            },
            {
              name: "form",
              value: "MOZSBR",
              purpose: "searchbar",
            },
            {
              name: "form",
              value: "MOZSPG",
              purpose: "homepage",
            },
            {
              name: "form",
              value: "MOZLBR",
              purpose: "keyword",
            },
            {
              name: "form",
              value: "MOZTSB",
              purpose: "newtab",
            },
          ],
          mozparams: {},
        },
        {
          type: "application/x-suggestions+json",
          method: "GET",
          template: "https://www.bing.com/osjson.aspx",
          params: [
            {
              name: "query",
              value: "{searchTerms}",
              purpose: undefined,
            },
            {
              name: "form",
              value: "OSDJAS",
              purpose: undefined,
            },
            {
              name: "language",
              value: "{moz:locale}",
              purpose: undefined,
            },
          ],
        },
      ],
    },
  };

  isSubObjectOf(EXPECTED_ENGINE, engine, "Bing");
});
