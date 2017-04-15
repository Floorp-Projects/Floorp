/**
 * Bug 1277803 - A test caes for testing favicon loading across different userContextId.
 */

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");

const TEST_SITE = "http://example.net";
const TEST_THIRD_PARTY_SITE = "http://mochi.test:8888";

const TEST_PAGE = TEST_SITE + "/browser/browser/components/originattributes/" +
                  "test/browser/file_favicon.html";
const FAVICON_URI = TEST_SITE + "/browser/browser/components/originattributes/" +
                    "test/browser/file_favicon.png";
const TEST_THIRD_PARTY_PAGE = "http://example.com/browser/browser/components/" +
                              "originattributes/test/browser/file_favicon_thirdParty.html";
const THIRD_PARTY_FAVICON_URI = TEST_THIRD_PARTY_SITE + "/browser/browser/components/" +
                                "originattributes/test/browser/file_favicon.png";

const USER_CONTEXT_ID_PERSONAL = 1;
const USER_CONTEXT_ID_WORK     = 2;

let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
let makeURI = Cu.import("resource://gre/modules/BrowserUtils.jsm", {}).BrowserUtils.makeURI;

function clearAllImageCaches() {
  var tools = SpecialPowers.Cc["@mozilla.org/image/tools;1"]
                             .getService(SpecialPowers.Ci.imgITools);
  var imageCache = tools.getImgCacheForDocument(window.document);
  imageCache.clearCache(true);  // true=chrome
  imageCache.clearCache(false); // false=content
}

function clearAllPlacesFavicons() {
  let faviconService = Cc["@mozilla.org/browser/favicon-service;1"]
                          .getService(Ci.nsIFaviconService);

  return new Promise(resolve => {
    let observer = {
      observe(aSubject, aTopic, aData) {
        if (aTopic === "places-favicons-expired") {
          resolve();
          Services.obs.removeObserver(observer, "places-favicons-expired");
        }
      }
    };

    Services.obs.addObserver(observer, "places-favicons-expired");
    faviconService.expireAllFavicons();
  });
}

function FaviconObserver(aUserContextId, aExpectedCookie, aPageURI, aFaviconURL) {
  this.reset(aUserContextId, aExpectedCookie, aPageURI, aFaviconURL);
}

FaviconObserver.prototype = {
  observe(aSubject, aTopic, aData) {
    // Make sure that the topic is 'http-on-modify-request'.
    if (aTopic === "http-on-modify-request") {
      // We check the userContextId for the originAttributes of the loading
      // channel. All requests for the favicon should contain the correct
      // userContextId. There are two requests for a favicon loading, one
      // from the Places library and one from the XUL image. The difference
      // of them is the loading principal. The Places will use the content
      // principal and the XUL image will use the system principal.

      let httpChannel = aSubject.QueryInterface(Ci.nsIHttpChannel);
      let reqLoadInfo = httpChannel.loadInfo;
      let loadingPrincipal;
      let triggeringPrincipal;

      // Make sure this is a favicon request.
      if (httpChannel.URI.spec !== this._faviconURL) {
        return;
      }

      if (reqLoadInfo) {
        loadingPrincipal = reqLoadInfo.loadingPrincipal;
        triggeringPrincipal = reqLoadInfo.triggeringPrincipal;
      }

      // Check the userContextId.
      is(reqLoadInfo.originAttributes.userContextId, this._curUserContextId,
        "The loadInfo has correct userContextId");

      if (loadingPrincipal.equals(systemPrincipal)) {
        this._faviconReqXUL = true;
        ok(triggeringPrincipal.equals(this._expectedPrincipal),
          "The triggeringPrincipal of favicon loading from XUL should be the content principal.");
      } else {
        this._faviconReqPlaces = true;
        ok(loadingPrincipal.equals(this._expectedPrincipal),
          "The loadingPrincipal of favicon loading from Places should be the content prinicpal");
      }

      let faviconCookie = httpChannel.getRequestHeader("cookie");

      is(faviconCookie, this._expectedCookie, "The cookie of the favicon loading is correct.");
    } else {
      ok(false, "Received unexpected topic: ", aTopic);
    }

    if (this._faviconReqXUL && this._faviconReqPlaces) {
      this._faviconLoaded.resolve();
    }
  },

  reset(aUserContextId, aExpectedCookie, aPageURI, aFaviconURL) {
    this._curUserContextId = aUserContextId;
    this._expectedCookie = aExpectedCookie;
    this._expectedPrincipal = Services.scriptSecurityManager
                                      .createCodebasePrincipal(aPageURI, { userContextId: aUserContextId });
    this._faviconReqXUL = false;
    this._faviconReqPlaces = false;
    this._faviconURL = aFaviconURL;
    this._faviconLoaded = new Promise.defer();
  },

  get promise() {
    return this._faviconLoaded.promise;
  }
};

function waitOnFaviconLoaded(aFaviconURL) {
  return new Promise(resolve => {
    let observer = {
      onPageChanged(uri, attr, value, id) {

        if (attr === Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON &&
            value === aFaviconURL) {
          resolve();
          PlacesUtils.history.removeObserver(observer, false);
        }
      },
    };

    PlacesUtils.history.addObserver(observer);
  });
}

function* generateCookies(aHost) {
  // we generate two different cookies for two userContextIds.
  let cookies = [];
  cookies.push(Math.random().toString());
  cookies.push(Math.random().toString());

  // Then, we add cookies into the site for 'personal' and 'work'.
  let tabInfoA = yield openTabInUserContext(aHost, USER_CONTEXT_ID_PERSONAL);
  let tabInfoB = yield openTabInUserContext(aHost, USER_CONTEXT_ID_WORK);

  yield ContentTask.spawn(tabInfoA.browser, cookies[0], function* (value) {
    content.document.cookie = value;
  });

  yield ContentTask.spawn(tabInfoB.browser, cookies[1], function* (value) {
    content.document.cookie = value;
  });

  yield BrowserTestUtils.removeTab(tabInfoA.tab);
  yield BrowserTestUtils.removeTab(tabInfoB.tab);

  return cookies;
}

function* doTest(aTestPage, aFaviconHost, aFaviconURL) {
  let cookies = yield generateCookies(aFaviconHost);
  let pageURI = makeURI(aTestPage);

  // Create the observer object for observing request channels of the personal
  // container.
  let observer = new FaviconObserver(USER_CONTEXT_ID_PERSONAL, cookies[0], pageURI, aFaviconURL);

  // Add the observer earlier in case we miss it.
  let promiseWaitOnFaviconLoaded = waitOnFaviconLoaded(aFaviconURL);

  Services.obs.addObserver(observer, "http-on-modify-request");

  // Open the tab with the personal container.
  let tabInfo = yield openTabInUserContext(aTestPage, USER_CONTEXT_ID_PERSONAL);

  // Waiting for favicon requests are all made.
  yield observer.promise;
  // Waiting for favicon loaded.
  yield promiseWaitOnFaviconLoaded;

  // Close the tab.
  yield BrowserTestUtils.removeTab(tabInfo.tab);

  // Reset the observer for observing requests for the work container.
  observer.reset(USER_CONTEXT_ID_WORK, cookies[1], pageURI, aFaviconURL);
  tabInfo = yield openTabInUserContext(aTestPage, USER_CONTEXT_ID_WORK);

  // Waiting for favicon requests are all made.
  yield observer.promise;

  Services.obs.removeObserver(observer, "http-on-modify-request");

  yield BrowserTestUtils.removeTab(tabInfo.tab);
}

add_task(function* setup() {
  // Make sure userContext is enabled.
  yield SpecialPowers.pushPrefEnv({"set": [
      ["privacy.userContext.enabled", true]
  ]});
});

// A clean up function to prevent affecting other tests.
registerCleanupFunction(() => {
  // Clear all cookies.
  let cookieMgr = Cc["@mozilla.org/cookiemanager;1"]
                     .getService(Ci.nsICookieManager);
  cookieMgr.removeAll();

  // Clear all image caches and network caches.
  clearAllImageCaches();

  let networkCache = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
                        .getService(Ci.nsICacheStorageService);
  networkCache.clear();

  // Clear Places favicon caches.
  clearAllPlacesFavicons();
});

add_task(function* test_favicon_userContextId() {
  // Clear all image caches before running the test.
  clearAllImageCaches();

  // Clear all network caches.
  let networkCache = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
                        .getService(Ci.nsICacheStorageService);
  networkCache.clear();

  // Clear Places favicon caches.
  yield clearAllPlacesFavicons();

  yield doTest(TEST_PAGE, TEST_SITE, FAVICON_URI);
});

add_task(function* test_thirdPartyFavicon_userContextId() {
  // Clear all image caches before running the test.
  clearAllImageCaches();

  // Clear all network caches.
  let networkCache = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
                        .getService(Ci.nsICacheStorageService);
  networkCache.clear();

  // Clear Places favicon caches.
  yield clearAllPlacesFavicons();

  yield doTest(TEST_THIRD_PARTY_PAGE, TEST_THIRD_PARTY_SITE, THIRD_PARTY_FAVICON_URI);
});
