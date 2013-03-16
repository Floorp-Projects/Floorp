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
      "_iconURL": "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAC0klEQVRYw8VX60sUURSffybqP+hDXzaXjF5UQm+C6ksERQ+iIIiiZXXTsqeVQmUWSVSkJlGUgQXWh8welqFWipala9o4dx47j9M5d5xxXXbbWXdm98BlWfbu/f3Ouef8zrmCgLYwMjpvcVS8XxIR/5ZEJQh0IQZhESZhC4si4vxQRJwIHDhlESZhC9zzAoM7i7CFgoQ9cxSmhLn8cXe9DC2dOvT/NkFSLUhnA2MmHG5Usp6VE4FVpxi0dev88IO3FVh9msHeBgW+/DQ5qG4AxFpU2FLDYEmFtzM9E6AD3w0aoOoA68+zWb8tr5Tg26hNQlQs2HCBeXbKM4HqRyoHePvdSPv7/puKG/7mzoT/BD4N2x4+wyvItGcwbu8ZEy3/CSgJ+G8EaDW9sTfJWoAEGGb90lj6PXXPNb5naNz0nwB57lhFs5p2T8NLm+X1ds1/AkfuzCQZ3fHaM7MzvRSr5CvqAuXKspOS/wRo1b/QXBJUdodQC9ZUM9h5VYZXfQa87jdgZRXLSdRyVsI9NxR4/F7n3vb9MsGcFsKeHyaUxnJXVSEfLd98kbkRaexIzOmMvAhsSiIwLlmwo04uLAFadO+OUf2feKAGRyBcLsGuazJceqrxbtjRa/CaTzYrqDIsx9ofmbSAoZekeMfuqbC9VoayswzKsCRJ/5Ottk3zj4AjsZ8x01M7YSpJffpG6HPdOZY/gaN3bQEyMNIbPbTZyoeqG4Vok5o/gfYenR9Gnc7rddFVkdFwkjcBpw3/wTILeyTgTEheyjIrgdYu3Q1pVWt2jygpE3j/3UOGP0m49bLstmLdtLM7k+TSjNg1YMAEs3iF+FYFB24pEJ+amX4nEeDJB52ToVGtBnWBvtOE/BE933ZF9l8HVmCXIzDqetSOaTilUqPc6B0xuQ7sw7kwnLMUF/thUvSnWeh4fEGxHqeE7b6QiU0oKoqBAyMGYfGXMdo//lt3GYGEQycAAAAASUVORK5CYII=",
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
