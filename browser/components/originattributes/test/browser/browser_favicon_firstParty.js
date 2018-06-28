/**
 * Bug 1277803 - A test case for testing favicon loading across different first party domains.
 */

const CC = Components.Constructor;

ChromeUtils.import("resource://gre/modules/PlacesUtils.jsm");

let EventUtils = {};
Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

const FIRST_PARTY_ONE = "example.com";
const FIRST_PARTY_TWO = "example.org";
const THIRD_PARTY = "mochi.test:8888";

const TEST_SITE_ONE = "http://" + FIRST_PARTY_ONE;
const TEST_SITE_TWO = "http://" + FIRST_PARTY_TWO;
const THIRD_PARTY_SITE = "http://" + THIRD_PARTY;
const TEST_DIRECTORY = "/browser/browser/components/originattributes/test/browser/";

const TEST_PAGE = TEST_DIRECTORY + "file_favicon.html";
const TEST_THIRD_PARTY_PAGE = TEST_DIRECTORY + "file_favicon_thirdParty.html";
const TEST_CACHE_PAGE = TEST_DIRECTORY + "file_favicon_cache.html";

const FAVICON_URI = TEST_DIRECTORY + "file_favicon.png";
const TEST_FAVICON_CACHE_URI = TEST_DIRECTORY + "file_favicon_cache.png";

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

function observeFavicon(aFirstPartyDomain, aExpectedCookie, aPageURI, aOnlyXUL) {
  let faviconReqXUL = false;
  // If aOnlyXUL is true, we only care about the favicon request from XUL.
  let faviconReqPlaces = aOnlyXUL === true;
  let expectedPrincipal = Services.scriptSecurityManager
                                  .createCodebasePrincipal(aPageURI, { firstPartyDomain: aFirstPartyDomain });

  return new Promise(resolve => {
    let observer = {
      observe(aSubject, aTopic, aData) {
        // Make sure that the topic is 'http-on-modify-request'.
        if (aTopic === "http-on-modify-request") {
          // We check the firstPartyDomain for the originAttributes of the loading
          // channel. All requests for the favicon should contain the correct
          // firstPartyDomain. There are two requests for a favicon loading, one
          // from the Places library and one from the XUL image. The difference
          // of them is the loading principal. The Places will use the content
          // principal and the XUL image will use the system principal.

          let httpChannel = aSubject.QueryInterface(Ci.nsIHttpChannel);
          let reqLoadInfo = httpChannel.loadInfo;
          let loadingPrincipal = reqLoadInfo.loadingPrincipal;
          let triggeringPrincipal = reqLoadInfo.triggeringPrincipal;

          // Make sure this is a favicon request.
          if (!httpChannel.URI.spec.endsWith(FAVICON_URI)) {
            return;
          }

          // Check the first party domain.
          is(reqLoadInfo.originAttributes.firstPartyDomain, aFirstPartyDomain,
            "The loadInfo has correct first party domain");

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
          Services.obs.removeObserver(observer, "http-on-modify-request");
          resolve();
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
            firstPartyDomain: loadInfo.originAttributes.firstPartyDomain
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

async function openTab(aURL) {
  let tab = BrowserTestUtils.addTab(gBrowser, aURL);

  // Select tab and make sure its browser is focused.
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return {tab, browser};
}

async function assignCookiesUnderFirstParty(aURL, aFirstParty, aCookieValue) {
  // Open a tab under the given aFirstParty, and this tab will have an
  // iframe which loads the aURL.
  let tabInfo = await openTabInFirstParty(aURL, aFirstParty);

  // Add cookies into the iframe.
  await ContentTask.spawn(tabInfo.browser, aCookieValue, async function(value) {
    content.document.cookie = value;
  });

  BrowserTestUtils.removeTab(tabInfo.tab);
}

async function generateCookies(aThirdParty) {
  // we generate two different cookies for two first party domains.
  let cookies = [];
  cookies.push(Math.random().toString());
  cookies.push(Math.random().toString());

  let firstSiteURL;
  let secondSiteURL;

  if (aThirdParty) {
    // Add cookies into the third party site with different first party domain.
    firstSiteURL = THIRD_PARTY_SITE;
    secondSiteURL = THIRD_PARTY_SITE;
  } else {
    // Add cookies into sites.
    firstSiteURL = TEST_SITE_ONE;
    secondSiteURL = TEST_SITE_TWO;
  }

  await assignCookiesUnderFirstParty(firstSiteURL, TEST_SITE_ONE, cookies[0]);
  await assignCookiesUnderFirstParty(secondSiteURL, TEST_SITE_TWO, cookies[1]);

  return cookies;
}

async function doTest(aTestPage, aExpectedCookies, aFaviconURL) {
  let firstPageURI = makeURI(TEST_SITE_ONE + aTestPage);
  let secondPageURI = makeURI(TEST_SITE_TWO + aTestPage);

  // Start to observe the event of that favicon has been fully loaded.
  let promiseFaviconLoaded = waitOnFaviconLoaded(aFaviconURL);

  // Start to observe the favicon requests earlier in case we miss it.
  let promiseObserveFavicon = observeFavicon(FIRST_PARTY_ONE, aExpectedCookies[0], firstPageURI);

  // Open the tab for the first site.
  let tabInfo = await openTab(TEST_SITE_ONE + aTestPage);

  // Waiting until favicon requests are all made.
  await promiseObserveFavicon;

  // Waiting until favicon loaded.
  await promiseFaviconLoaded;

  BrowserTestUtils.removeTab(tabInfo.tab);
  // FIXME: We need to wait for the next event tick here to avoid observing
  //        the previous tab info in the next step (bug 1446725).
  await new Promise(executeSoon);

  // Start to observe the favicon requests earlier in case we miss it.
  promiseObserveFavicon = observeFavicon(FIRST_PARTY_TWO, aExpectedCookies[1], secondPageURI);

  // Open the tab for the second site.
  tabInfo = await openTab(TEST_SITE_TWO + aTestPage);

  // Waiting until favicon requests are all made.
  await promiseObserveFavicon;

  BrowserTestUtils.removeTab(tabInfo.tab);
}

async function doTestForAllTabsFavicon(aTestPage, aExpectedCookies, aIsThirdParty) {
  let firstPageURI = makeURI(TEST_SITE_ONE + aTestPage);
  let secondPageURI = makeURI(TEST_SITE_TWO + aTestPage);
  let faviconURI = aIsThirdParty ? THIRD_PARTY_SITE + FAVICON_URI :
                                   TEST_SITE_ONE + FAVICON_URI;

  // Set the 'overflow' attribute to make allTabs button available.
  let tabBrowser = document.getElementById("tabbrowser-tabs");
  tabBrowser.setAttribute("overflow", true);

  // Start to observe the event of that the favicon has been fully loaded.
  let promiseFaviconLoaded = waitOnFaviconLoaded(faviconURI);

  // Open the tab for the first site.
  let tabInfo = await openTab(TEST_SITE_ONE + aTestPage);

  // Waiting until the favicon loaded.
  await promiseFaviconLoaded;

  // We need to clear the image cache here for making sure the network request will
  // be made for the favicon of allTabs menuitem.
  clearAllImageCaches();

  // Start to observe the allTabs favicon requests earlier in case we miss it.
  let promiseObserveFavicon = observeFavicon(FIRST_PARTY_ONE, aExpectedCookies[0],
                                             firstPageURI, true);

  // Make the popup of allTabs showing up and trigger the loading of the favicon.
  let allTabsView = document.getElementById("allTabsMenu-allTabsView");
  let allTabsPopupShownPromise = BrowserTestUtils.waitForEvent(allTabsView, "ViewShown");
  gTabsPanel.showAllTabsPanel();
  await promiseObserveFavicon;
  await allTabsPopupShownPromise;

  // Close the popup of allTabs and wait until it's done.
  let allTabsPopupHiddenPromise = BrowserTestUtils.waitForEvent(allTabsView.panelMultiView, "PanelMultiViewHidden");
  gTabsPanel.hideAllTabsPanel();
  await allTabsPopupHiddenPromise;

  // Close the tab.
  BrowserTestUtils.removeTab(tabInfo.tab);

  faviconURI = aIsThirdParty ? THIRD_PARTY_SITE + FAVICON_URI :
                               TEST_SITE_TWO + FAVICON_URI;

  // Start to observe the event of that favicon has been fully loaded.
  promiseFaviconLoaded = waitOnFaviconLoaded(faviconURI);

  // Open the tab for the second site.
  tabInfo = await openTab(TEST_SITE_TWO + aTestPage);

  // Wait until the favicon is fully loaded.
  await promiseFaviconLoaded;

  // Clear the image cache for the favicon of the second site.
  clearAllImageCaches();

  // Start to observe the allTabs favicon requests earlier in case we miss it.
  promiseObserveFavicon = observeFavicon(FIRST_PARTY_TWO, aExpectedCookies[1],
                                         secondPageURI, true);

  // Make the popup of allTabs showing up again.
  allTabsPopupShownPromise = BrowserTestUtils.waitForEvent(allTabsView, "ViewShown");
  gTabsPanel.showAllTabsPanel();
  await promiseObserveFavicon;
  await allTabsPopupShownPromise;

  // Close the popup of allTabs and wait until it's done.
  allTabsPopupHiddenPromise = BrowserTestUtils.waitForEvent(allTabsView.panelMultiView, "PanelMultiViewHidden");
  gTabsPanel.hideAllTabsPanel();
  await allTabsPopupHiddenPromise;

  // Close the tab.
  BrowserTestUtils.removeTab(tabInfo.tab);

  // Reset the 'overflow' attribute to make the allTabs button hidden again.
  tabBrowser.removeAttribute("overflow");
}

add_task(async function setup() {
  // Make sure first party isolation is enabled.
  await SpecialPowers.pushPrefEnv({"set": [
      ["privacy.firstparty.isolate", true]
  ]});
});

// A clean up function to prevent affecting other tests.
registerCleanupFunction(() => {
  // Clear all cookies.
  Services.cookies.removeAll();

  // Clear all image caches and network caches.
  clearAllImageCaches();

  Services.cache2.clear();
});

add_task(async function test_favicon_firstParty() {
  for (let testThirdParty of [false, true]) {
    // Clear all image caches and network caches before running the test.
    clearAllImageCaches();

    Services.cache2.clear();

    // Clear Places favicon caches.
    await clearAllPlacesFavicons();

    let cookies = await generateCookies(testThirdParty);

    if (testThirdParty) {
      await doTest(TEST_THIRD_PARTY_PAGE, cookies, THIRD_PARTY_SITE + FAVICON_URI);
    } else {
      await doTest(TEST_PAGE, cookies, TEST_SITE_ONE + FAVICON_URI);
    }
  }
});

add_task(async function test_allTabs_favicon_firstParty() {
  for (let testThirdParty of [false, true]) {
    // Clear all image caches and network caches before running the test.
    clearAllImageCaches();

    Services.cache2.clear();

    // Clear Places favicon caches.
    await clearAllPlacesFavicons();

    let cookies = await generateCookies(testThirdParty);

    if (testThirdParty) {
      await doTestForAllTabsFavicon(TEST_THIRD_PARTY_PAGE, cookies, testThirdParty);
    } else {
      await doTestForAllTabsFavicon(TEST_PAGE, cookies, testThirdParty);
    }
  }
});

add_task(async function test_favicon_cache_firstParty() {
  // Clear all image caches and network caches before running the test.
  clearAllImageCaches();

  Services.cache2.clear();

  // Start to observer the event of that favicon has been fully loaded and cached.
  let promiseForFaviconLoaded = waitOnFaviconLoaded(THIRD_PARTY_SITE + TEST_FAVICON_CACHE_URI);

  // Start to observer for the favicon response of the first tab.
  let responsePromise = waitOnFaviconResponse(THIRD_PARTY_SITE + TEST_FAVICON_CACHE_URI);

  // Open the tab for the first site.
  let tabInfoA = await openTab(TEST_SITE_ONE + TEST_CACHE_PAGE);

  // Waiting for the favicon response.
  let response = await responsePromise;

  // Make sure the favicon is loaded through the network and its first party domain is correct.
  is(response.topic, "http-on-examine-response", "The favicon image should be loaded through network.");
  is(response.firstPartyDomain, FIRST_PARTY_ONE, "We should only observe the network response for the first first party.");

  // Waiting until the favicon has been loaded and cached.
  await promiseForFaviconLoaded;

  // Open the tab again for checking the image cache is working correctly.
  let tabInfoB = await openTab(TEST_SITE_ONE + TEST_CACHE_PAGE);

  // Start to observe the favicon response, the second tab actually will not
  // make any network request since the favicon will be loaded by the cache for
  // both Places and XUL image. So here, we are going to observe the favicon
  // response for the third tab which opens with the second first party.
  let promiseForFaviconResponse = waitOnFaviconResponse(THIRD_PARTY_SITE + TEST_FAVICON_CACHE_URI);

  // Open the tab for the second site.
  let tabInfoC = await openTab(TEST_SITE_TWO + TEST_CACHE_PAGE);

  // Wait for the favicon response. In this case, we suppose to catch the
  // response for the third tab but not the second tab since it will not
  // go through the network.
  response = await promiseForFaviconResponse;

  // Check that the favicon response has came from the network and it has the
  // correct first party domain.
  is(response.topic, "http-on-examine-response", "The favicon image should be loaded through network again.");
  is(response.firstPartyDomain, FIRST_PARTY_TWO, "We should only observe the network response for the second first party.");

  BrowserTestUtils.removeTab(tabInfoA.tab);
  BrowserTestUtils.removeTab(tabInfoB.tab);
  BrowserTestUtils.removeTab(tabInfoC.tab);
});
