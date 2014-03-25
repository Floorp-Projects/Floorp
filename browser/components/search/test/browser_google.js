/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test Google search plugin URLs
 */

"use strict";

const BROWSER_SEARCH_PREF      = "browser.search.";

const MOZ_PARAM_LOCALE         = /\{moz:locale\}/g;
const MOZ_PARAM_DIST_ID        = /\{moz:distributionID\}/g;
const MOZ_PARAM_OFFICIAL       = /\{moz:official\}/g;

let runtime = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
// Custom search parameters
const MOZ_OFFICIAL = runtime.isOfficialBranding ? "official" : "unofficial";

var google_client;
switch (runtime.defaultUpdateChannel) {
case "beta":
  google_client = "firefox-beta";
  break;
case "aurora":
  google_client = "firefox-aurora";
  break;
case "nightly":
  google_client = "firefox-nightly";
  break;
default:
  google_client = "firefox-a";
  break;
}

const GOOGLE_CLIENT = google_client;
const MOZ_DISTRIBUTION_ID = runtime.distributionID;

function test() {
  let engine = Services.search.getEngineByName("Google");
  ok(engine, "Google");

  is(Services.search.defaultEngine, engine, "Check that Google is the default search engine");

  let distributionID;
  try {
    distributionID = Services.prefs.getCharPref(BROWSER_SEARCH_PREF + "distributionID");
  } catch (ex) {
    distributionID = MOZ_DISTRIBUTION_ID;
  }

  let base = "https://www.google.com/search?q=foo&ie=utf-8&oe=utf-8&aq=t&rls={moz:distributionID}:{moz:locale}:{moz:official}&client=" + GOOGLE_CLIENT;
  base = base.replace(MOZ_PARAM_LOCALE, getLocale());
  base = base.replace(MOZ_PARAM_DIST_ID, distributionID);
  base = base.replace(MOZ_PARAM_OFFICIAL, MOZ_OFFICIAL);

  let url;

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, base, "Check search URL for 'foo'");
  url = engine.getSubmission("foo", null, "contextmenu").uri.spec;
  is(url, base + "&channel=rcs", "Check context menu search URL for 'foo'");
  url = engine.getSubmission("foo", null, "keyword").uri.spec;
  is(url, base + "&channel=fflb", "Check keyword search URL for 'foo'");
  url = engine.getSubmission("foo", null, "searchbar").uri.spec;
  is(url, base + "&channel=sb", "Check search bar search URL for 'foo'");
  url = engine.getSubmission("foo", null, "homepage").uri.spec;
  is(url, base + "&channel=np&source=hp", "Check homepage search URL for 'foo'");

  // Check search suggestion URL.
  url = engine.getSubmission("foo", "application/x-suggestions+json").uri.spec;
  is(url, "https://www.google.com/complete/search?client=firefox&q=foo", "Check search suggestion URL for 'foo'");

  // Check all other engine properties.
  const EXPECTED_ENGINE = {
    name: "Google",
    alias: null,
    description: "Google Search",
    searchForm: "https://www.google.com/",
    type: Ci.nsISearchEngine.TYPE_MOZSEARCH,
    hidden: false,
    wrappedJSObject: {
      queryCharset: "UTF-8",
      "_iconURL": "data:image/x-icon;base64,AAABAAIAEBAAAAAAAAB9AQAAJgAAACAgAAAAAAAA8gIAAKMBAACJUE5HDQoaCgAAAA1JSERSAAAAEAAAABAIBgAAAB/z/2EAAAFESURBVDjLpZNJSwNBEIXnt4lE4kHxovgT9BDwJHqPy0HEEOJBiAuCRg+KUdC4QS4KrpC4gCBGE3NQ48JsnZ6eZ3UOM6gjaePhQU93v6+qq2q0pqgeJj2S8EdJT1hr0OxBtKCD5iEd8QxDYpvhvOBAuMDKURX9C9aPu4GA1GEVkzvMg10UBfYveWAWgYAP00V01fa+R9M2bA51wJvhIn3qR+ybt3D3JNQBE5sMjCIOLFpoHzOwdsLRO22qA6R6kiZiWwxUvy/PUQZIhYZ1vFM9cvcOOsYNdcBgysISdSJBnZjJMlR0Fw8vAp0xoz5gao/h+NZBy4i/10XGwrPA+hmvDyhVRG2Avu/LwcrkFADZa16L1h330w1RNgc3DiJzCpPYRm1bpveXX11clQR28xwblHpk1vq1iP/5mcoS0CoXDZiL0vsJ+dzfl+3T/VYAAAAASUVORK5CYIKJUE5HDQoaCgAAAA1JSERSAAAAIAAAACAIBgAAAHN6evQAAAK5SURBVFjDxVfrSxRRFJ9/Jta/oyWjF5XQm6D6EkHRgygIIgjUTcueVgqVWSRRkppEUQYWWB8ye1iGWilWlo/Ude489s7M6Zw7D9dlt53dmd29cFiWvXvO77x+51xpaaUsoSxBaUWZQ4ECy5xji2xKZDyCMlMEw6lCNiOSgwZKJK1SkcKeSealfP64t0mBjl4Ow39MkDUL0p2RSROOtqhZdeUEYM1pBl39XCg/fEeFtWcY7G9W4csvUxjlBkCsQ4Nt9QyWVfvT6RsAKXw3aoDGATZeYIt+W1kjw7cJG0RctWDTRebbKd8A6h5pwsDb70ba3w/eUr3wt/cmwgfw6Yft4TNMQaY7o1P2ncm4FT4ANQH/jQBJ2xv7kqIXEADDql8eS3+n8bku7oxNm+EDIM/dU92upb3T/NJGeaNbDx/AsbsLRUY5Xn92caWXY5d8RV6gWllxSg4fAEnTC90DQW13BLlgXR2D3dcUeDVkwOthA1bXspxILWcm3HdThcfvufB26LcJpkOEAz9NKI/lzqpSEC7feol5EWnpSeSlIxCALUkApmULdjUqxQVAQnl3D/X/yQda4QBEq2TYc12By091MQ17Bg3R88nHKlQbVmHvj89awNBLYrwT9zXY2aBAxTkGFdiSxP/Jp6FLDw+AS7GfsdJTJ2EqSO5khD43nGfBARy/ZxOQgZHe7GPM1jzUvChUtmnBAXQPcKGMJp3fdFGq6NByEhiAO4b/YptFfQJwNyQ/bZkVQGcf90Ja25ndIyrKBOa/f8wIpwi3X1G8UcxNu7ozUS7tiH0jBswwS3RIaF1w6LYKU/ML2+8sGnjygQswtKrVIy/Qd9qQP6LnO64q4fPAKpxyZIymHo1jWk6p1ag2BsdNwQMHcC+M5kHFJX+YlPxpVlbCx2mZ5DzPI04k4kUwHHdskU3pH76iftG8yWlkAAAAAElFTkSuQmCC",
      _urls : [
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
            {
              "name": "ie",
              "value": "utf-8",
              "purpose": undefined,
            },
            {
              "name": "oe",
              "value": "utf-8",
              "purpose": undefined,
            },
            {
              "name": "aq",
              "value": "t",
              "purpose": undefined,
            },
            {
              "name": "rls",
              "value": "{moz:distributionID}:{moz:locale}:{moz:official}",
              "purpose": undefined,
            },
            {
              "name": "client",
              "value": GOOGLE_CLIENT,
              "purpose": undefined,
            },
            {
              "name": "channel",
              "value": "rcs",
              "purpose": "contextmenu",
            },
            {
              "name": "channel",
              "value": "fflb",
              "purpose": "keyword",
            },
            {
              "name": "channel",
              "value": "sb",
              "purpose": "searchbar",
            },
            {
              "name": "channel",
              "value": "np",
              "purpose": "homepage",
            },
            {
              "name": "source",
              "value": "hp",
              "purpose": "homepage",
            },
          ],
          mozparams: {
            "client": {
              "name": "client",
              "falseValue": "firefox",
              "trueValue": GOOGLE_CLIENT,
              "condition": "defaultEngine",
              "mozparam": true,
            },
          },
        },
        {
          type: "text/html",
          method: "GET",
          template: "https://www.google.com/",
        },
      ],
    },
  };

  isSubObjectOf(EXPECTED_ENGINE, engine, "Google");
}
