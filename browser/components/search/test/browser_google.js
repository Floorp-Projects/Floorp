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
      "_iconURL": "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAACuUlEQVRYw8VX60sUURSffybWv6MloxeV0Jug+hJB0YMoCCII1E3LnlYKlVkkUZKaRFEGFlgfMntYhlopVpaP1HXuPPbOzOmcOw/XZbed3ZndvXBYlr17zu+8fudcaWmlLKEsQWlFmUOBAsucY4tsSmQ8gjJTBMOpQjYjkoMGSiStUpHCnknmpXz+uLdJgY5eDsN/TJA1C9KdkUkTjraoWXXlBGDNaQZd/VwoP3xHhbVnGOxvVuHLL1MY5QZArEODbfUMllX70+kbACl8N2qAxgE2XmCLfltZI8O3CRtEXLVg00Xm2ynfAOoeacLA2+9G2t8P3lK98Lf3JsIH8OmH7eEzTEGmO6NT9p3JuBU+ADUB/40ASdsb+5KiFxAAw6pfHkt/p/G5Lu6MTZvhAyDP3VPdrqW90/zSRnmjWw8fwLG7C0VGOV5/dnGll2OXfEVeoFpZcUoOHwBJ0wvdA0FtdwS5YF0dg93XFHg1ZMDrYQNW17KcSC1nJtx3U4XH77nwdui3CaZDhAM/TSiP5c6qUhAu33qJeRFp6UnkpSMQgC1JAKZlC3Y1KsUFQEJ5dw/1/8kHWuEARKtk2HNdgctPdTENewYN0fPJxypUG1Zh74/PWsDQS2K8E/c12NmgQMU5BhXYksT/yaehSw8PgEuxn7HSUydhKkjuZIQ+N5xnwQEcv2cTkIGR3uxjzNY81LwoVLZpwQF0D3ChjCad33RRqujQchIYgDuG/2KbRX0CcDckP22ZFUBnH/dCWtuZ3SMqygTmv3/MCKcIt19RvFHMTbu6M1Eu7Yh9IwbMMEt0SGhdcOi2ClPzC9vvLBp48oELMLSq1SMv0HfakD+i5zuuKuHzwCqccmSMph6NY1pOqdWoNgbHTcEDB3AvjOZBxSV/mJT8aVZWwsdpmeQ8zyNOJOJFMBx3bJFN6R++on7RvMlpZAAAAABJRU5ErkJggg==",
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
