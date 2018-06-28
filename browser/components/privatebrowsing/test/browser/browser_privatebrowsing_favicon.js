/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test make sure that the favicon of the private browsing is isolated.

const TEST_SITE = "http://mochi.test:8888";
const TEST_CACHE_SITE = "http://www.example.com";
const TEST_DIRECTORY = "/browser/browser/components/privatebrowsing/test/browser/";

const TEST_PAGE = TEST_SITE + TEST_DIRECTORY + "file_favicon.html";
const TEST_CACHE_PAGE = TEST_CACHE_SITE + TEST_DIRECTORY + "file_favicon.html";
const FAVICON_URI = TEST_SITE + TEST_DIRECTORY + "file_favicon.png";
const FAVICON_CACHE_URI = TEST_CACHE_SITE + TEST_DIRECTORY + "file_favicon.png";

let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
let makeURI = ChromeUtils.import("resource://gre/modules/BrowserUtils.jsm", {}).BrowserUtils.makeURI;

function clearAllImageCaches() {
  let tools = SpecialPowers.Cc["@mozilla.org/image/tools;1"]
                             .getService(SpecialPowers.Ci.imgITools);
  let imageCache = tools.getImgCacheForDocument(window.document);
  imageCache.clearCache(true); // true=chrome
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

function observeFavicon(aIsPrivate, aExpectedCookie, aPageURI) {
  let faviconReqXUL = false;
  let faviconReqPlaces = false;
  let attr = {};

  if (aIsPrivate) {
    attr.privateBrowsingId = 1;
  }

  let expectedPrincipal = Services.scriptSecurityManager
                                  .createCodebasePrincipal(aPageURI, attr);

  return new Promise(resolve => {
    let observer = {
      observe(aSubject, aTopic, aData) {
        // Make sure that the topic is 'http-on-modify-request'.
        if (aTopic === "http-on-modify-request") {
          // We check the privateBrowsingId for the originAttributes of the loading
          // channel. All requests for the favicon should contain the correct
          // privateBrowsingId. There are two requests for a favicon loading, one
          // from the Places library and one from the XUL image. The difference
          // of them is the loading principal. The Places will use the content
          // principal and the XUL image will use the system principal.

          let httpChannel = aSubject.QueryInterface(Ci.nsIHttpChannel);
          let reqLoadInfo = httpChannel.loadInfo;
          let loadingPrincipal = reqLoadInfo.loadingPrincipal;
          let triggeringPrincipal = reqLoadInfo.triggeringPrincipal;

          // Make sure this is a favicon request.
          if (httpChannel.URI.spec !== FAVICON_URI) {
            return;
          }

          // Check the privateBrowsingId.
          if (aIsPrivate) {
            is(reqLoadInfo.originAttributes.privateBrowsingId, 1, "The loadInfo has correct privateBrowsingId");
          } else {
            is(reqLoadInfo.originAttributes.privateBrowsingId, 0, "The loadInfo has correct privateBrowsingId");
          }

          if (loadingPrincipal.equals(systemPrincipal)) {
            faviconReqXUL = true;
            ok(triggeringPrincipal.equals(expectedPrincipal),
              "The triggeringPrincipal of favicon loading from XUL should be the content principal.");
          } else {
            faviconReqPlaces = true;
            ok(loadingPrincipal.equals(expectedPrincipal),
              "The loadingPrincipal of favicon loading from Places should be the content prinicpal");
          }

          let faviconCookie = httpChannel.getRequestHeader("cookie");

          is(faviconCookie, aExpectedCookie, "The cookie of the favicon loading is correct.");
        } else {
          ok(false, "Received unexpected topic: ", aTopic);
        }

        if (faviconReqXUL && faviconReqPlaces) {
          resolve();
          Services.obs.removeObserver(observer, "http-on-modify-request");
        }
      }
    };

    Services.obs.addObserver(observer, "http-on-modify-request");
  });
}

function waitOnFaviconResponse(aFaviconURL) {
  return new Promise(resolve => {
    let observer = {
      observe(aSubject, aTopic, aData) {
        if (aTopic === "http-on-examine-response" ||
            aTopic === "http-on-examine-cached-response") {

          let httpChannel = aSubject.QueryInterface(Ci.nsIHttpChannel);
          let loadInfo = httpChannel.loadInfo;

          if (httpChannel.URI.spec !== aFaviconURL) {
            return;
          }

          let result = {
            topic: aTopic,
            privateBrowsingId: loadInfo.originAttributes.privateBrowsingId
          };

          resolve(result);
          Services.obs.removeObserver(observer, "http-on-examine-response");
          Services.obs.removeObserver(observer, "http-on-examine-cached-response");
        }
      }
    };

    Services.obs.addObserver(observer, "http-on-examine-response");
    Services.obs.addObserver(observer, "http-on-examine-cached-response");
  });
}

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

async function assignCookies(aBrowser, aURL, aCookieValue) {
  let tabInfo = await openTab(aBrowser, aURL);

  await ContentTask.spawn(tabInfo.browser, aCookieValue, async function(value) {
    content.document.cookie = value;
  });

  BrowserTestUtils.removeTab(tabInfo.tab);
}

async function openTab(aBrowser, aURL) {
  let tab = aBrowser.addTab(aURL);

  // Select tab and make sure its browser is focused.
  aBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = aBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return {tab, browser};
}

registerCleanupFunction(async () => {
  Services.cookies.removeAll();
  clearAllImageCaches();
  Services.cache2.clear();
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_favicon_privateBrowsing() {
  // Clear all image caches before running the test.
  clearAllImageCaches();
  // Clear all favicons in Places.
  await clearAllPlacesFavicons();

  // Create a private browsing window.
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let pageURI = makeURI(TEST_PAGE);

  // Generate two random cookies for non-private window and private window
  // respectively.
  let cookies = [];
  cookies.push(Math.random().toString());
  cookies.push(Math.random().toString());

  // Open a tab in private window and add a cookie into it.
  await assignCookies(privateWindow.gBrowser, TEST_SITE, cookies[0]);

  // Open a tab in non-private window and add a cookie into it.
  await assignCookies(gBrowser, TEST_SITE, cookies[1]);

  // Add the observer earlier in case we don't capture events in time.
  let promiseObserveFavicon = observeFavicon(true, cookies[0], pageURI);

  // The page must be bookmarked for favicon requests to go through in PB mode.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_PAGE
  });

  // Open a tab for the private window.
  let tabInfo = await openTab(privateWindow.gBrowser, TEST_PAGE);

  info("Waiting until favicon requests are all made in private window.");
  await promiseObserveFavicon;

  // Close the tab.
  BrowserTestUtils.removeTab(tabInfo.tab);
  // FIXME: We need to wait for the next event tick here to avoid observing
  //        the previous tab info in the next step (bug 1446725).
  await new Promise(executeSoon);

  // Add the observer earlier in case we don't capture events in time.
  promiseObserveFavicon = observeFavicon(false, cookies[1], pageURI);

  // Open a tab for the non-private window.
  tabInfo = await openTab(gBrowser, TEST_PAGE);

  info("Waiting until favicon requests are all made in non-private window.");
  await promiseObserveFavicon;

  // Close the tab.
  BrowserTestUtils.removeTab(tabInfo.tab);
  await BrowserTestUtils.closeWindow(privateWindow);
});

add_task(async function test_favicon_cache_privateBrowsing() {
  // Clear all image caches and network cache before running the test.
  clearAllImageCaches();

  Services.cache2.clear();

  // Clear all favicons in Places.
  await clearAllPlacesFavicons();

  // Add an observer for making sure the favicon has been loaded and cached.
  let promiseFaviconLoaded = waitOnFaviconLoaded(FAVICON_CACHE_URI);

  // Open a tab for the non-private window.
  let tabInfoNonPrivate = await openTab(gBrowser, TEST_CACHE_PAGE);

  let response = await waitOnFaviconResponse(FAVICON_CACHE_URI);

  await promiseFaviconLoaded;

  // Check that the favicon response has come from the network and it has the
  // correct privateBrowsingId.
  is(response.topic, "http-on-examine-response", "The favicon image should be loaded through network.");
  is(response.privateBrowsingId, 0, "We should observe the network response for the non-private tab.");

  // Create a private browsing window.
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  // The page must be bookmarked for favicon requests to go through in PB mode.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_CACHE_PAGE
  });

  // Open a tab for the private window.
  let tabInfoPrivate = await openTab(privateWindow.gBrowser, TEST_CACHE_PAGE);

  // Wait for the favicon response of the private tab.
  response = await waitOnFaviconResponse(FAVICON_CACHE_URI);

  // Make sure the favicon is loaded through the network and its privateBrowsingId is correct.
  is(response.topic, "http-on-examine-response", "The favicon image should be loaded through the network again.");
  is(response.privateBrowsingId, 1, "We should observe the network response for the private tab.");

  BrowserTestUtils.removeTab(tabInfoPrivate.tab);
  BrowserTestUtils.removeTab(tabInfoNonPrivate.tab);
  await BrowserTestUtils.closeWindow(privateWindow);
});
