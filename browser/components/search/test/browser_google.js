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

// Custom search parameters
#ifdef MOZ_OFFICIAL_BRANDING
const MOZ_OFFICIAL = "official";
#else
const MOZ_OFFICIAL = "unofficial";
#endif

#if MOZ_UPDATE_CHANNEL == beta
const GOOGLE_CLIENT = "firefox-beta";
#elif MOZ_UPDATE_CHANNEL == aurora
const GOOGLE_CLIENT = "firefox-aurora";
#elif MOZ_UPDATE_CHANNEL == nightly
const GOOGLE_CLIENT = "firefox-nightly";
#else
const GOOGLE_CLIENT = "firefox-a";
#endif

#expand const MOZ_DISTRIBUTION_ID = __MOZ_DISTRIBUTION_ID__;

function getLocale() {
  const localePref = "general.useragent.locale";
  return getLocalizedPref(localePref, Services.prefs.getCharPref(localePref));
}

/**
 * Wrapper for nsIPrefBranch::getComplexValue.
 * @param aPrefName
 *        The name of the pref to get.
 * @returns aDefault if the requested pref doesn't exist.
 */
function getLocalizedPref(aPrefName, aDefault) {
  try {
    return Services.prefs.getComplexValue(aPrefName, Ci.nsIPrefLocalizedString).data;
  } catch (ex) {
    return aDefault;
  }

  return aDefault;
}

function test() {
  let engine = Services.search.getEngineByName("Google");
  ok(engine, "Google");

  is(Services.search.originalDefaultEngine, engine, "Check that Google is the default search engine");

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
      "_iconURL": "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAABUUlEQVR42pWTzUsCYRCH9y9zu3SooCCkjhIRRLeIykXokiWCJ7PvDpZRlz6si1lIQZ3SQxQdOhREpgSm0JeQvfu0+i6I7LKLh4F5h5nnnRl+o6jTdHn8omAYbVqhXqvYFXcEBKFDwcoZZB8B4LkEB9cwGGmFKHb01A1EU9JXzfdvDYZi1lwLwBcVAIwsNWPesIwls7gDtB2Z7N9ujVe+IX2LO2AgItB1OL9vJqsmILDrOoK02IkBAdYy4FsQJC5h+VQCHQDWTqYSgo8fuHuRxS4Ae3stQ7UGE5ttAHqCUgfxC7m4ryrowOyeO6CxqHwZxtYFqtYc5+kNan/gDTsAeueEIRj7n/rmRQMwueUAGF0VAAT3rQBTC0Y3DoDOGbm00icML4oWHYSTgo0MFqjlmPpDgqMcFCuQf4erBzjOwXjcriu9qHg0uutO2+es6fl67T9ptebvFRjBVgAAAABJRU5ErkJggg==",
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
      ],
    },
  };

  isSubObjectOf(EXPECTED_ENGINE, engine, "Google");
}
