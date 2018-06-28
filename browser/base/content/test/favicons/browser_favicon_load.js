/**
 * Bug 1247843 - A test case for testing whether the channel used to load favicon
 * has correct classFlags.
 * Note that this test is modified based on browser_favicon_userContextId.js.
 */

const CC = Components.Constructor;

const TEST_SITE = "http://example.net";
const TEST_THIRD_PARTY_SITE = "http://mochi.test:8888";

const TEST_PAGE =
  TEST_SITE + "/browser/browser/base/content/test/favicons/file_favicon.html";
const FAVICON_URI =
  TEST_SITE + "/browser/browser/base/content/test/favicons/file_favicon.png";
const TEST_THIRD_PARTY_PAGE =
  TEST_SITE + "/browser/browser/base/content/test/favicons/file_favicon_thirdParty.html";
const THIRD_PARTY_FAVICON_URI =
  TEST_THIRD_PARTY_SITE + "/browser/browser/base/content/test/favicons/file_favicon.png";

ChromeUtils.defineModuleGetter(this, "PromiseUtils",
                               "resource://gre/modules/PromiseUtils.jsm");
ChromeUtils.defineModuleGetter(this, "PlacesTestUtils",
                               "resource://testing-common/PlacesTestUtils.jsm");

let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

function clearAllImageCaches() {
  var tools = Cc["@mozilla.org/image/tools;1"]
                .getService(Ci.imgITools);
  var imageCache = tools.getImgCacheForDocument(window.document);
  imageCache.clearCache(true); // true=chrome
  imageCache.clearCache(false); // false=content
}

function clearAllPlacesFavicons() {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer() {
      Services.obs.removeObserver(observer, "places-favicons-expired");
      resolve();
    }, "places-favicons-expired");

    PlacesUtils.favicons.expireAllFavicons();
  });
}

function FaviconObserver(aPageURI, aFaviconURL, aTailingEnabled) {
  this.reset(aPageURI, aFaviconURL, aTailingEnabled);
}

FaviconObserver.prototype = {
  observe(aSubject, aTopic, aData) {
    // Make sure that the topic is 'http-on-modify-request'.
    if (aTopic === "http-on-modify-request") {
      let httpChannel = aSubject.QueryInterface(Ci.nsIHttpChannel);
      let reqLoadInfo = httpChannel.loadInfo;
      // Make sure this is a favicon request.
      if (httpChannel.URI.spec !== this._faviconURL) {
        return;
      }

      let cos = aSubject.QueryInterface(Ci.nsIClassOfService);
      if (!cos) {
        ok(false, "Http channel should implement nsIClassOfService.");
        return;
      }

      if (!reqLoadInfo) {
        ok(false, "Should have load info.");
        return;
      }

      let haveTailFlag = !!(cos.classFlags & Ci.nsIClassOfService.Tail);
      info("classFlags=" + cos.classFlags);
      is(haveTailFlag, this._tailingEnabled, "Should have correct cos flag.");
    } else {
      ok(false, "Received unexpected topic: ", aTopic);
    }

    this._faviconLoaded.resolve();
  },

  reset(aPageURI, aFaviconURL, aTailingEnabled) {
    this._faviconURL = aFaviconURL;
    this._faviconLoaded = PromiseUtils.defer();
    this._tailingEnabled = aTailingEnabled;
  },

  get promise() {
    return this._faviconLoaded.promise;
  }
};

function waitOnFaviconLoaded(aFaviconURL) {
  return PlacesTestUtils.waitForNotification(
    "onPageChanged",
    (uri, attr, value, id) => attr === Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON &&
                              value === aFaviconURL,
    "history");
}

async function doTest(aTestPage, aFaviconURL, aTailingEnabled) {
  let pageURI = Services.io.newURI(aTestPage);

  // Create the observer object for observing favion channels.
  let observer = new FaviconObserver(pageURI, aFaviconURL, aTailingEnabled);

  let promiseWaitOnFaviconLoaded = waitOnFaviconLoaded(aFaviconURL);

  // Add the observer earlier in case we miss it.
  Services.obs.addObserver(observer, "http-on-modify-request");

  // Open the tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, aTestPage);
  // Waiting for favicon requests are all made.
  await observer.promise;
  // Waiting for favicon loaded.
  await promiseWaitOnFaviconLoaded;

  Services.obs.removeObserver(observer, "http-on-modify-request");

  // Close the tab.
  BrowserTestUtils.removeTab(tab);
}

async function setupTailingPreference(aTailingEnabled) {
  await SpecialPowers.pushPrefEnv({"set": [
      ["network.http.tailing.enabled", aTailingEnabled]
  ]});
}

async function cleanup() {
  // Clear all cookies.
  Services.cookies.removeAll();
  // Clear cache.
  Services.cache2.clear();
  // Clear Places favicon caches.
  await clearAllPlacesFavicons();
  // Clear all image caches and network caches.
  clearAllImageCaches();
  // Clear Places history.
  await PlacesUtils.history.clear();
}

// A clean up function to prevent affecting other tests.
registerCleanupFunction(async () => {
  await cleanup();
});

add_task(async function test_favicon_with_tailing_enabled() {
  await cleanup();

  let tailingEnabled = true;

  await setupTailingPreference(tailingEnabled);

  await doTest(TEST_PAGE, FAVICON_URI, tailingEnabled);
});

add_task(async function test_favicon_with_tailing_disabled() {
  await cleanup();

  let tailingEnabled = false;

  await setupTailingPreference(tailingEnabled);

  await doTest(TEST_THIRD_PARTY_PAGE, THIRD_PARTY_FAVICON_URI, tailingEnabled);
});
