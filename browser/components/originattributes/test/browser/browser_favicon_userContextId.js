/**
 * Bug 1277803 - A test caes for testing favicon loading across different userContextId.
 */

if (SpecialPowers.useRemoteSubframes) {
  requestLongerTimeout(2);
}

let EventUtils = {};
Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
  EventUtils
);

ChromeUtils.defineModuleGetter(
  this,
  "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm"
);

const TEST_SITE = "http://example.net";
const TEST_THIRD_PARTY_SITE = "http://mochi.test:8888";

const TEST_PAGE =
  TEST_SITE +
  "/browser/browser/components/originattributes/" +
  "test/browser/file_favicon.html";
const FAVICON_URI =
  TEST_SITE +
  "/browser/browser/components/originattributes/" +
  "test/browser/file_favicon.png";
const TEST_THIRD_PARTY_PAGE =
  "http://example.com/browser/browser/components/" +
  "originattributes/test/browser/file_favicon_thirdParty.html";
const THIRD_PARTY_FAVICON_URI =
  TEST_THIRD_PARTY_SITE +
  "/browser/browser/components/" +
  "originattributes/test/browser/file_favicon.png";

const USER_CONTEXT_ID_PERSONAL = 1;
const USER_CONTEXT_ID_WORK = 2;

let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

function clearAllImageCaches() {
  var tools = SpecialPowers.Cc["@mozilla.org/image/tools;1"].getService(
    SpecialPowers.Ci.imgITools
  );
  var imageCache = tools.getImgCacheForDocument(window.document);
  imageCache.clearCache(true); // true=chrome
  imageCache.clearCache(false); // false=content
}

function clearAllPlacesFavicons() {
  let faviconService = Cc["@mozilla.org/browser/favicon-service;1"].getService(
    Ci.nsIFaviconService
  );

  return new Promise(resolve => {
    let observer = {
      observe(aSubject, aTopic, aData) {
        if (aTopic === "places-favicons-expired") {
          resolve();
          Services.obs.removeObserver(observer, "places-favicons-expired");
        }
      },
    };

    Services.obs.addObserver(observer, "places-favicons-expired");
    faviconService.expireAllFavicons();
  });
}

function FaviconObserver(
  aUserContextId,
  aExpectedCookie,
  aPageURI,
  aFaviconURL
) {
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
      let loadingPrincipal, triggeringPrincipal;

      // Make sure this is a favicon request.
      if (httpChannel.URI.spec !== this._faviconURL) {
        return;
      }

      if (reqLoadInfo) {
        loadingPrincipal = reqLoadInfo.loadingPrincipal;
        triggeringPrincipal = reqLoadInfo.triggeringPrincipal;
      }

      // Check the userContextId.
      is(
        reqLoadInfo.originAttributes.userContextId,
        this._curUserContextId,
        "The loadInfo has correct userContextId"
      );

      ok(
        loadingPrincipal.equals(this._expectedPrincipal),
        "The loadingPrincipal of favicon loading from content should be the content prinicpal"
      );
      ok(
        triggeringPrincipal.equals(this._expectedPrincipal),
        "The triggeringPrincipal of favicon loading from content should be the content prinicpal"
      );

      let faviconCookie = httpChannel.getRequestHeader("cookie");

      is(
        faviconCookie,
        this._expectedCookie,
        "The cookie of the favicon loading is correct."
      );
    } else {
      ok(false, "Received unexpected topic: ", aTopic);
    }

    this._faviconLoaded.resolve();
  },

  reset(aUserContextId, aExpectedCookie, aPageURI, aFaviconURL) {
    this._curUserContextId = aUserContextId;
    this._expectedCookie = aExpectedCookie;
    this._expectedPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      aPageURI,
      { userContextId: aUserContextId }
    );
    this._faviconURL = aFaviconURL;
    this._faviconLoaded = PromiseUtils.defer();
  },

  get promise() {
    return this._faviconLoaded.promise;
  },
};

function waitOnFaviconLoaded(aFaviconURL) {
  return PlacesTestUtils.waitForNotification(
    "favicon-changed",
    events => events.some(e => e.faviconUrl == aFaviconURL),
    "places"
  );
}

async function generateCookies(aHost) {
  // we generate two different cookies for two userContextIds.
  let cookies = [];
  cookies.push(Math.random().toString());
  cookies.push(Math.random().toString());

  // Then, we add cookies into the site for 'personal' and 'work'.
  let tabInfoA = await openTabInUserContext(aHost, USER_CONTEXT_ID_PERSONAL);
  let tabInfoB = await openTabInUserContext(aHost, USER_CONTEXT_ID_WORK);

  await SpecialPowers.spawn(tabInfoA.browser, [cookies[0]], async function(
    value
  ) {
    content.document.cookie = value;
  });

  await SpecialPowers.spawn(tabInfoB.browser, [cookies[1]], async function(
    value
  ) {
    content.document.cookie = value;
  });

  BrowserTestUtils.removeTab(tabInfoA.tab);
  BrowserTestUtils.removeTab(tabInfoB.tab);

  return cookies;
}

async function doTest(aTestPage, aFaviconHost, aFaviconURL) {
  let cookies = await generateCookies(aFaviconHost);
  let pageURI = makeURI(aTestPage);

  // Create the observer object for observing request channels of the personal
  // container.
  let observer = new FaviconObserver(
    USER_CONTEXT_ID_PERSONAL,
    cookies[0],
    pageURI,
    aFaviconURL
  );

  // Add the observer earlier in case we miss it.
  let promiseWaitOnFaviconLoaded = waitOnFaviconLoaded(aFaviconURL);

  Services.obs.addObserver(observer, "http-on-modify-request");

  // Open the tab with the personal container.
  let tabInfo = await openTabInUserContext(aTestPage, USER_CONTEXT_ID_PERSONAL);

  // Waiting for favicon requests are all made.
  await observer.promise;
  // Waiting for favicon loaded.
  await promiseWaitOnFaviconLoaded;

  // Close the tab.
  BrowserTestUtils.removeTab(tabInfo.tab);
  // FIXME: We need to wait for the next event tick here to avoid observing
  //        the previous tab info in the next step (bug 1446725).
  await new Promise(executeSoon);

  // Reset the observer for observing requests for the work container.
  observer.reset(USER_CONTEXT_ID_WORK, cookies[1], pageURI, aFaviconURL);
  tabInfo = await openTabInUserContext(aTestPage, USER_CONTEXT_ID_WORK);

  // Waiting for favicon requests are all made.
  await observer.promise;

  Services.obs.removeObserver(observer, "http-on-modify-request");

  BrowserTestUtils.removeTab(tabInfo.tab);
}

function assertIconIsData(item) {
  is(
    item.getAttribute("image").substring(0, 5),
    "data:",
    "Expected the image element to be a data URI"
  );
}

async function doTestForAllTabsFavicon(aTestPage, aFaviconHost, aFaviconURL) {
  await generateCookies(aFaviconHost);

  // Set the 'overflow' attribute to make allTabs button available.
  let tabBrowser = document.getElementById("tabbrowser-tabs");
  tabBrowser.setAttribute("overflow", true);

  // Add the observer earlier in case we miss it.
  let promiseWaitOnFaviconLoaded = waitOnFaviconLoaded(aFaviconURL);

  // Open the tab with the personal container.
  let tabInfo = await openTabInUserContext(aTestPage, USER_CONTEXT_ID_PERSONAL);

  // Waiting for favicon loaded.
  await promiseWaitOnFaviconLoaded;

  // We need to clear the image cache here for making sure the network request will
  // be made for the favicon of allTabs menuitem.
  clearAllImageCaches();

  gTabsPanel.init();

  // Make the popup of allTabs showing up and trigger the loading of the favicon.
  let allTabsView = document.getElementById("allTabsMenu-allTabsView");
  let allTabsPopupShownPromise = BrowserTestUtils.waitForEvent(
    allTabsView,
    "ViewShown"
  );
  gTabsPanel.showAllTabsPanel();
  await allTabsPopupShownPromise;

  assertIconIsData(
    gTabsPanel.allTabsViewTabs.lastElementChild.firstElementChild
  );

  // Close the popup of allTabs and wait until it's done.
  let allTabsPopupHiddenPromise = BrowserTestUtils.waitForEvent(
    allTabsView.panelMultiView,
    "PanelMultiViewHidden"
  );
  gTabsPanel.hideAllTabsPanel();
  await allTabsPopupHiddenPromise;

  // Close the tab.
  BrowserTestUtils.removeTab(tabInfo.tab);
  // FIXME: We need to wait for the next event tick here to avoid observing
  //        the previous tab info in the next step (bug 1446725).
  await new Promise(executeSoon);

  // Open the tab under the work container and wait until the favicon is loaded.
  promiseWaitOnFaviconLoaded = waitOnFaviconLoaded(aFaviconURL);
  tabInfo = await openTabInUserContext(aTestPage, USER_CONTEXT_ID_WORK);
  await promiseWaitOnFaviconLoaded;

  // Clear the image cache again.
  clearAllImageCaches();

  // Make the popup of allTabs showing up again.
  allTabsPopupShownPromise = BrowserTestUtils.waitForEvent(
    allTabsView,
    "ViewShown"
  );
  gTabsPanel.showAllTabsPanel();
  await allTabsPopupShownPromise;

  assertIconIsData(
    gTabsPanel.allTabsViewTabs.lastElementChild.firstElementChild
  );

  // Close the popup of allTabs and wait until it's done.
  allTabsPopupHiddenPromise = BrowserTestUtils.waitForEvent(
    allTabsView.panelMultiView,
    "PanelMultiViewHidden"
  );
  gTabsPanel.hideAllTabsPanel();
  await allTabsPopupHiddenPromise;

  // Close the tab.
  BrowserTestUtils.removeTab(tabInfo.tab);

  // Reset the 'overflow' attribute to make the allTabs button hidden again.
  tabBrowser.removeAttribute("overflow");
}

add_task(async function setup() {
  // Make sure userContext is enabled.
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });
});

// A clean up function to prevent affecting other tests.
registerCleanupFunction(() => {
  // Clear all cookies.
  Services.cookies.removeAll();

  // Clear all image caches and network caches.
  clearAllImageCaches();

  Services.cache2.clear();

  // Clear Places favicon caches.
  clearAllPlacesFavicons();
});

add_task(async function test_favicon_userContextId() {
  // Clear all image caches before running the test.
  clearAllImageCaches();

  // Clear all network caches.
  Services.cache2.clear();

  // Clear Places favicon caches.
  await clearAllPlacesFavicons();

  await doTest(TEST_PAGE, TEST_SITE, FAVICON_URI);
});

add_task(async function test_thirdPartyFavicon_userContextId() {
  // Clear all image caches before running the test.
  clearAllImageCaches();

  // Clear all network caches.
  Services.cache2.clear();

  // Clear Places favicon caches.
  await clearAllPlacesFavicons();

  await doTest(
    TEST_THIRD_PARTY_PAGE,
    TEST_THIRD_PARTY_SITE,
    THIRD_PARTY_FAVICON_URI
  );
});

add_task(async function test_allTabs_favicon_userContextId() {
  // Clear all image caches before running the test.
  clearAllImageCaches();

  // Clear all network caches.
  Services.cache2.clear();

  // Clear Places favicon caches.
  await clearAllPlacesFavicons();

  await doTestForAllTabsFavicon(TEST_PAGE, TEST_SITE, FAVICON_URI);
});

add_task(async function test_allTabs_thirdPartyFavicon_userContextId() {
  // Clear all image caches before running the test.
  clearAllImageCaches();

  // Clear all network caches.
  Services.cache2.clear();

  // Clear Places favicon caches.
  await clearAllPlacesFavicons();

  await doTestForAllTabsFavicon(
    TEST_THIRD_PARTY_PAGE,
    TEST_THIRD_PARTY_SITE,
    THIRD_PARTY_FAVICON_URI
  );
});
