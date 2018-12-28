/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test Google search plugin URLs
 */

"use strict";

let expectedEngine = {
  name: "Google",
  alias: null,
  description: "Google Search",
  searchForm: "https://www.google.com/search?",
  hidden: false,
  wrappedJSObject: {
    queryCharset: "UTF-8",
    "_iconURL": "resource://search-plugins/images/google.ico",
    _urls: [
      {
        type: "application/x-suggestions+json",
        method: "GET",
        template: "https://www.google.com/complete/search?client=firefox&q={searchTerms}",
        params: "",
      },
      {
        type: "text/html",
        method: "GET",
        template: "https://www.google.com/search",
        params: [
          {
            "name": "q",
            "value": "{searchTerms}",
            "purpose": undefined,
          },
        ],
        mozparams: {
        },
      },
    ],
  },
};

function test() {
  let engine = Services.search.getEngineByName("Google");
  ok(engine, "Found Google search engine");

  let region = Services.prefs.getCharPref("browser.search.region");
  let code = "";
  switch (region) {
    case "US":
      code = "firefox-b-1-d";
      break;
    case "DE":
      code = "firefox-b-d";
      break;
    case "RU":
      // Covered by test but doesn't use a code
      break;
  }

  if (code) {
    expectedEngine.searchForm += `client=${code}&`;
    let urlParams = expectedEngine.wrappedJSObject._urls[1].params;
    urlParams.unshift({
      name: "client",
      value: code,
    });
  }
  expectedEngine.searchForm += "q=";

  let url;

  // Test search URLs (including purposes).
  let purposes = ["", "contextmenu", "searchbar", "homepage", "newtab", "keyword"];
  let urlParams;
  for (let purpose of purposes) {
    url = engine.getSubmission("foo", null, purpose).uri.spec;
    urlParams = new URLSearchParams(url.split("?")[1]);
    is(urlParams.get("client"), code, "Check ${purpose} search URL for code");
    is(urlParams.get("q"), "foo", `Check ${purpose} search URL for 'foo'`);
  }

  // Check search suggestion URL.
  url = engine.getSubmission("foo", "application/x-suggestions+json").uri.spec;
  is(url, "https://www.google.com/complete/search?client=firefox&q=foo", "Check search suggestion URL for 'foo'");

  // Check result parsing and alternate domains.
  let base = "https://www.google.com/search?q=foo";
  is(Services.search.parseSubmissionURL(base).terms, "foo",
     "Check result parsing");
  let alternateBase = base.replace("www.google.com", "www.google.fr");
  is(Services.search.parseSubmissionURL(alternateBase).terms, "foo",
     "Check alternate domain");

  // Check all other engine properties.
  isSubObjectOf(expectedEngine, engine, "Google");
}
