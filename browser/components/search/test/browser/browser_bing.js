/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test Bing search plugin URLs
 */

"use strict";

add_task(async function test() {
  await Services.search.init();

  let engine = Services.search.getEngineByName("Bing");
  ok(engine, "Bing");

  let base = "https://www.bing.com/search?form={code}&pc=MOZI&q=foo";
  let url;

  function getUrl(code) {
    return base.replace("{code}", code);
  }

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, getUrl("MOZSBR"), "Check search URL for 'foo'");
  url = engine.getSubmission("foo", null, "contextmenu").uri.spec;
  is(url, getUrl("MOZCON"), "Check context menu search URL for 'foo'");
  url = engine.getSubmission("foo", null, "keyword").uri.spec;
  is(url, getUrl("MOZLBR"), "Check keyword search URL for 'foo'");
  url = engine.getSubmission("foo", null, "searchbar").uri.spec;
  is(url, getUrl("MOZSBR"), "Check search bar search URL for 'foo'");
  url = engine.getSubmission("foo", null, "homepage").uri.spec;
  is(url, getUrl("MOZSPG"), "Check homepage search URL for 'foo'");
  url = engine.getSubmission("foo", null, "newtab").uri.spec;
  is(url, getUrl("MOZTSB"), "Check newtab search URL for 'foo'");

  // Check search suggestion URL.
  url = engine.getSubmission("foo", "application/x-suggestions+json").uri.spec;
  is(
    url,
    "https://www.bing.com/osjson.aspx?query=foo&form=OSDJAS&language=" +
      getLocale(),
    "Check search suggestion URL for 'foo'"
  );

  // Check all other engine properties.
  const EXPECTED_ENGINE = {
    name: "Bing",
    alias: null,
    description: "Bing. Search by Microsoft.",
    searchForm: "https://www.bing.com/search?pc=MOZI&q=",
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
            {
              name: "pc",
              value: "MOZI",
              purpose: undefined,
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
