/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kUrlPref = "geoSpecificDefaults.url";
const BROWSER_SEARCH_PREF = "browser.search.";

var originalGeoURL;
var originalCountryCode;
var originalRegion;

/**
 * Clean the profile of any cache file left from a previous run.
 * Returns a boolean indicating if the cache file existed.
 */
function removeCacheFile() {
  const CACHE_FILENAME = "search.json.mozlz4";

  let file =  Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append(CACHE_FILENAME);
  if (file.exists()) {
    file.remove(false);
    return true;
  }
  return false;
}

/**
 * Returns a promise that is resolved when an observer notification from the
 * search service fires with the specified data.
 *
 * @param aExpectedData
 *        The value the observer notification sends that causes us to resolve
 *        the promise.
 */
function waitForSearchNotification(aExpectedData, aCallback) {
  const SEARCH_SERVICE_TOPIC = "browser-search-service";
  Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
    if (aData != aExpectedData)
      return;

    Services.obs.removeObserver(observer, SEARCH_SERVICE_TOPIC);
    aCallback();
  }, SEARCH_SERVICE_TOPIC, false);
}

function asyncInit() {
  return new Promise(resolve => {
    Services.search.init(function() {
      ok(Services.search.isInitialized, "search service should be initialized");
      resolve();
    });
  });
}

function asyncReInit() {
  const kLocalePref = "general.useragent.locale";

  let promise = new Promise(resolve => {
    waitForSearchNotification("reinit-complete", resolve);
  });

  Services.search.QueryInterface(Ci.nsIObserver)
          .observe(null, "nsPref:changed", kLocalePref);

  return promise;
}

let gEngineCount;

add_task(function* preparation() {
  // ContentSearch is interferring with our async re-initializations of the
  // search service: once _initServicePromise has resolved, it will access
  // the search service, thus causing unpredictable behavior due to
  // synchronous initializations of the service.
  let originalContentSearchPromise = ContentSearch._initServicePromise;
  ContentSearch._initServicePromise = new Promise(resolve => {
    registerCleanupFunction(() => {
      ContentSearch._initServicePromise = originalContentSearchPromise;
      resolve();
    });
  });

  yield asyncInit();
  gEngineCount = Services.search.getVisibleEngines().length;

  waitForSearchNotification("uninit-complete", () => {
    // Verify search service is not initialized
    is(Services.search.isInitialized, false, "Search service should NOT be initialized");

    removeCacheFile();

    // Make sure we get the new country/region values, but save the old
    originalCountryCode = Services.prefs.getCharPref(BROWSER_SEARCH_PREF + "countryCode");
    originalRegion = Services.prefs.getCharPref(BROWSER_SEARCH_PREF + "region");
    Services.prefs.clearUserPref(BROWSER_SEARCH_PREF + "countryCode");
    Services.prefs.clearUserPref(BROWSER_SEARCH_PREF + "region");

    // Geo specific defaults won't be fetched if there's no country code.
    Services.prefs.setCharPref("browser.search.geoip.url",
                               'data:application/json,{"country_code": "US"}');

    Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", true);

    // Avoid going to the server for the geo lookup. We take the defaults
    originalGeoURL = Services.prefs.getCharPref(BROWSER_SEARCH_PREF + kUrlPref);
    let geoUrl = "data:application/json,{}";
    Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF).setCharPref(kUrlPref, geoUrl);
  });

  yield asyncReInit();

  yield new Promise(resolve => {
    waitForSearchNotification("write-cache-to-disk-complete", resolve);
  });
});

add_task(function* tests() {
  let engine = Services.search.getEngineByName("Google");
  ok(engine, "Google");

  let base = "https://www.google.com/search?q=foo&ie=utf-8&oe=utf-8";

  let url;

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo", null, "contextmenu").uri.spec;
  is(url, base, "Check context menu search URL for 'foo'");
  url = engine.getSubmission("foo", null, "keyword").uri.spec;
  is(url, base, "Check keyword search URL for 'foo'");
  url = engine.getSubmission("foo", null, "searchbar").uri.spec;
  is(url, base, "Check search bar search URL for 'foo'");
  url = engine.getSubmission("foo", null, "homepage").uri.spec;
  is(url, base, "Check homepage search URL for 'foo'");
  url = engine.getSubmission("foo", null, "newtab").uri.spec;
  is(url, base, "Check newtab search URL for 'foo'");
  url = engine.getSubmission("foo", null, "system").uri.spec;
  is(url, base, "Check system search URL for 'foo'");
});


add_task(function* cleanup() {
  waitForSearchNotification("uninit-complete", () => {
    // Verify search service is not initialized
    is(Services.search.isInitialized, false,
       "Search service should NOT be initialized");
    removeCacheFile();

    Services.prefs.clearUserPref("browser.search.geoip.url");

    Services.prefs.setCharPref(BROWSER_SEARCH_PREF + "countryCode", originalCountryCode)
    Services.prefs.setCharPref(BROWSER_SEARCH_PREF + "region", originalRegion)

    // We can't clear the pref because it's set to false by testing/profiles/prefs_general.js
    Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", false);

    Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF).setCharPref(kUrlPref, originalGeoURL);
  });

  yield asyncReInit();
  is(gEngineCount, Services.search.getVisibleEngines().length,
     "correct engine count after cleanup");
});
