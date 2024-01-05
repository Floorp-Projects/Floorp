/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the Search Tips feature, which displays a prompt to use the Urlbar on
// the newtab page and on the user's default search engine's homepage.
// Specifically, it tests that the Tips appear when they should be appearing.
// This doesn't test the max-shown-count limit or the restriction on tips when
// we show the default browser prompt because those require restarting the
// browser.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.sys.mjs",
  HttpServer: "resource://testing-common/httpd.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderSearchTips:
    "resource:///modules/UrlbarProviderSearchTips.sys.mjs",
});

// These should match the same consts in UrlbarProviderSearchTips.jsm.
const MAX_SHOWN_COUNT = 4;
const LAST_UPDATE_THRESHOLD_MS = 24 * 60 * 60 * 1000;

// We test some of the bigger Google domains.
const GOOGLE_DOMAINS = [
  "www.google.com",
  "www.google.ca",
  "www.google.co.uk",
  "www.google.com.au",
  "www.google.co.nz",
];

// In order for the persist tip to appear, the scheme of the
// search engine has to be the same as the scheme of the SERP url.
// withDNSRedirect() loads an http: url while the searchform
// of the default engine uses https. To enable the search term
// to be shown, we use the Example engine because it doesn't require
// a redirect.
const SEARCH_SERP_URL = "https://example.com/?q=chocolate";

add_setup(async function () {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        `browser.urlbar.tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.ONBOARD}`,
        0,
      ],
      [
        `browser.urlbar.tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.PERSIST}`,
        0,
      ],
      [
        `browser.urlbar.tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.REDIRECT}`,
        0,
      ],
      // Set following prefs so tips are actually shown.
      ["browser.laterrun.bookkeeping.profileCreationTime", 0],
      ["browser.laterrun.bookkeeping.updateAppliedTime", 0],
    ],
  });

  // Remove update history and the current active update so tips are shown.
  let updateRootDir = Services.dirsvc.get("UpdRootD", Ci.nsIFile);
  let updatesFile = updateRootDir.clone();
  updatesFile.append("updates.xml");
  let activeUpdateFile = updateRootDir.clone();
  activeUpdateFile.append("active-update.xml");
  try {
    updatesFile.remove(false);
  } catch (e) {}
  try {
    activeUpdateFile.remove(false);
  } catch (e) {}

  let defaultEngine = await Services.search.getDefault();
  let defaultEngineName = defaultEngine.name;
  Assert.equal(defaultEngineName, "Google", "Default engine should be Google.");

  // Add a mock engine so we don't hit the network loading the SERP.
  await SearchTestUtils.installSearchExtension();

  registerCleanupFunction(async () => {
    await setDefaultEngine(defaultEngineName);
    resetSearchTipsProvider();
  });
});

// The onboarding tip should be shown on about:newtab.
add_task(async function newtab() {
  await checkTab(
    window,
    "about:newtab",
    UrlbarProviderSearchTips.TIP_TYPE.ONBOARD
  );
});

// The onboarding tip should be shown on about:home.
add_task(async function home() {
  await checkTab(
    window,
    "about:home",
    UrlbarProviderSearchTips.TIP_TYPE.ONBOARD
  );
});

// The redirect tip should be shown for www.google.com when it's the default
// engine.
add_task(async function google() {
  await setDefaultEngine("Google");
  for (let domain of GOOGLE_DOMAINS) {
    await withDNSRedirect(domain, "/", async url => {
      await checkTab(window, url, UrlbarProviderSearchTips.TIP_TYPE.REDIRECT);
    });
  }
});

// The redirect tip should be shown for www.google.com/webhp when it's the
// default engine.
add_task(async function googleWebhp() {
  await setDefaultEngine("Google");
  for (let domain of GOOGLE_DOMAINS) {
    await withDNSRedirect(domain, "/webhp", async url => {
      await checkTab(window, url, UrlbarProviderSearchTips.TIP_TYPE.REDIRECT);
    });
  }
});

// The redirect tip should be shown for the Google homepage when query strings
// are appended.
add_task(async function googleQueryString() {
  await setDefaultEngine("Google");
  for (let domain of GOOGLE_DOMAINS) {
    await withDNSRedirect(domain, "/webhp", async url => {
      await checkTab(
        window,
        `${url}?hl=en`,
        UrlbarProviderSearchTips.TIP_TYPE.REDIRECT
      );
    });
  }
});

// The redirect tip should not be shown on Google results pages.
add_task(async function googleResults() {
  await setDefaultEngine("Google");
  for (let domain of GOOGLE_DOMAINS) {
    await withDNSRedirect(domain, "/search", async url => {
      await checkTab(
        window,
        `${url}?q=firefox`,
        UrlbarProviderSearchTips.TIP_TYPE.NONE
      );
    });
  }
});

// The redirect tip should not be shown for www.google.com when it's not the
// default engine.
add_task(async function googleNotDefault() {
  await setDefaultEngine("Bing");
  for (let domain of GOOGLE_DOMAINS) {
    await withDNSRedirect(domain, "/", async url => {
      await checkTab(window, url, UrlbarProviderSearchTips.TIP_TYPE.NONE);
    });
  }
});

// The redirect tip should not be shown for www.google.com/webhp when it's not
// the default engine.
add_task(async function googleWebhpNotDefault() {
  await setDefaultEngine("Bing");
  for (let domain of GOOGLE_DOMAINS) {
    await withDNSRedirect(domain, "/webhp", async url => {
      await checkTab(window, url, UrlbarProviderSearchTips.TIP_TYPE.NONE);
    });
  }
});

// The redirect tip should be shown for www.bing.com when it's the default
// engine.
add_task(async function bing() {
  await setDefaultEngine("Bing");
  await withDNSRedirect("www.bing.com", "/", async url => {
    await checkTab(window, url, UrlbarProviderSearchTips.TIP_TYPE.REDIRECT);
  });
});

// The redirect tip should be shown on the Bing homepage even when Bing appends
// query strings.
add_task(async function bingQueryString() {
  await setDefaultEngine("Bing");
  await withDNSRedirect("www.bing.com", "/", async url => {
    await checkTab(
      window,
      `${url}?toWww=1`,
      UrlbarProviderSearchTips.TIP_TYPE.REDIRECT
    );
  });
});

// The redirect tip should not be shown on Bing results pages.
add_task(async function bingResults() {
  await setDefaultEngine("Bing");
  await withDNSRedirect("www.bing.com", "/search", async url => {
    await checkTab(
      window,
      `${url}?q=firefox`,
      UrlbarProviderSearchTips.TIP_TYPE.NONE
    );
  });
});

// The redirect tip should not be shown for www.bing.com when it's not the
// default engine.
add_task(async function bingNotDefault() {
  await setDefaultEngine("Google");
  await withDNSRedirect("www.bing.com", "/", async url => {
    await checkTab(window, url, UrlbarProviderSearchTips.TIP_TYPE.NONE);
  });
});

// The redirect tip should be shown for duckduckgo.com when it's the default
// engine.
add_task(async function ddg() {
  await setDefaultEngine("DuckDuckGo");
  await withDNSRedirect("duckduckgo.com", "/", async url => {
    await checkTab(window, url, UrlbarProviderSearchTips.TIP_TYPE.REDIRECT);
  });
});

// The redirect tip should be shown for start.duckduckgo.com when it's the
// default engine.
add_task(async function ddgStart() {
  await setDefaultEngine("DuckDuckGo");
  await withDNSRedirect("start.duckduckgo.com", "/", async url => {
    await checkTab(window, url, UrlbarProviderSearchTips.TIP_TYPE.REDIRECT);
  });
});

// The redirect tip should not be shown for duckduckgo.com when it's not the
// default engine.
add_task(async function ddgNotDefault() {
  await setDefaultEngine("Google");
  await withDNSRedirect("duckduckgo.com", "/", async url => {
    await checkTab(window, url, UrlbarProviderSearchTips.TIP_TYPE.NONE);
  });
});

// The redirect tip should not be shown for start.duckduckgo.com when it's not
// the default engine.
add_task(async function ddgStartNotDefault() {
  await setDefaultEngine("Google");
  await withDNSRedirect("start.duckduckgo.com", "/", async url => {
    await checkTab(window, url, UrlbarProviderSearchTips.TIP_TYPE.NONE);
  });
});

// The redirect tip should not be shown for duckduckgo.com/?q=foo, the search
// results page, which happens to have the same domain and path as the home
// page.
add_task(async function ddgSearchResultsPage() {
  await setDefaultEngine("DuckDuckGo");
  await withDNSRedirect("duckduckgo.com", "/", async url => {
    await checkTab(
      window,
      `${url}?q=test`,
      UrlbarProviderSearchTips.TIP_TYPE.NONE
    );
  });
});

// The redirect tip should not be shown on a non-engine page.
add_task(async function nonEnginePage() {
  await checkTab(
    window,
    "http://example.com/",
    UrlbarProviderSearchTips.TIP_TYPE.NONE
  );
});

// The persist tip should show on default SERPs.
// This test also has an implied check that the SERP
// is receiving an originalURI.
// This is because the page the test is attempting to load
// will differ from the page that's actually loaded due to
// the DNS redirect.
add_task(async function persistTipOnDefault() {
  await setDefaultEngine("Example");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });
  await checkTab(
    window,
    SEARCH_SERP_URL,
    UrlbarProviderSearchTips.TIP_TYPE.PERSIST
  );
  await SpecialPowers.popPrefEnv();
});

// The persist tip should not show on non-default SERPs.
add_task(async function noPersistTipOnNonDefault() {
  await setDefaultEngine("DuckDuckGo");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });
  await checkTab(
    window,
    SEARCH_SERP_URL,
    UrlbarProviderSearchTips.TIP_TYPE.NONE
  );
  await SpecialPowers.popPrefEnv();
});

// The persist tip should only show up once a session.
add_task(async function persistTipOnceOnDefaultSerp() {
  await setDefaultEngine("Example");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });
  await checkTab(
    window,
    SEARCH_SERP_URL,
    UrlbarProviderSearchTips.TIP_TYPE.PERSIST
  );
  await checkTab(
    window,
    SEARCH_SERP_URL,
    UrlbarProviderSearchTips.TIP_TYPE.NONE
  );
  await SpecialPowers.popPrefEnv();
});

// The persist tip should not show in a window
// with a selected tab containing a non-SERP url.
add_task(async function noPersistTipInWindowWithNonSerpTab() {
  await setDefaultEngine("Example");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });

  // Create a new window for the SERP to be loaded into.
  let newWindow = await BrowserTestUtils.openNewBrowserWindow();

  // Focus on the original window.
  window.focus();
  await waitForBrowserWindowActive(window);

  // Load the SERP in the new window to initiate a background load.
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    newWindow.gBrowser.selectedBrowser,
    false,
    SEARCH_SERP_URL
  );
  BrowserTestUtils.startLoadingURIString(
    newWindow.gBrowser.selectedBrowser,
    SEARCH_SERP_URL
  );
  await browserLoadedPromise;

  // Wait longer than the persist tip delay to check that the search tip
  // doesn't show on the non-SERP tab.
  await new Promise(resolve =>
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, UrlbarProviderSearchTips.SHOW_PERSIST_TIP_DELAY_MS * 2)
  );
  Assert.ok(!window.gURLBar.view.isOpen);

  // Clean up.
  await BrowserTestUtils.closeWindow(newWindow);
  await SpecialPowers.popPrefEnv();
  resetSearchTipsProvider();
});

// Tips should be shown at most once per session regardless of their type.
add_task(async function oncePerSession() {
  await setDefaultEngine("Google");
  await checkTab(
    window,
    "about:newtab",
    UrlbarProviderSearchTips.TIP_TYPE.ONBOARD,
    false
  );
  await checkTab(
    window,
    "about:newtab",
    UrlbarProviderSearchTips.TIP_TYPE.NONE,
    false
  );
  await withDNSRedirect("www.google.com", "/", async url => {
    await checkTab(window, url, UrlbarProviderSearchTips.TIP_TYPE.NONE);
  });
  await setDefaultEngine("Example");
  await checkTab(
    window,
    SEARCH_SERP_URL,
    UrlbarProviderSearchTips.TIP_TYPE.NONE
  );
});

// The one-off search buttons should not be shown when
// a search tip is shown even though the search string is empty.
add_task(async function shortcut_buttons_with_tip() {
  await checkTab(
    window,
    "about:newtab",
    UrlbarProviderSearchTips.TIP_TYPE.ONBOARD
  );
});

// Don't show the persist search tip when the browser loads
// a different page from the page the tip was supposed to show on.
add_task(async function noSearchTipWhileAnotherPageLoads() {
  await setDefaultEngine("Example");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });

  // Create a slow endpoint.
  const SLOW_PAGE =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "http://www.example.com"
    ) + "slow-page.sjs";

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: SEARCH_SERP_URL,
  });

  // Load a slow URI to cause an onStateChange event but
  // not an onLocationChange event.
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, SLOW_PAGE);

  // Wait roughly for the amount of time it would take for the
  // persist search tip to show.
  await new Promise(resolve =>
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, UrlbarProviderSearchTips.SHOW_PERSIST_TIP_DELAY_MS * 2)
  );

  // Check the search tip didn't show while the page was loading.
  Assert.equal(
    UrlbarPrefs.get(
      `tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.PERSIST}`
    ),
    0,
    "The shownCount pref should be 0."
  );

  Assert.equal(false, window.gURLBar.view.isOpen, "Urlbar should be closed.");

  // Clean up.
  await SpecialPowers.popPrefEnv();
  resetSearchTipsProvider();
  BrowserTestUtils.removeTab(tab);
});

// Show the persist search tip when the browser is still loading
// resources from the page the tip is supposed to show on.
add_task(async function searchTipWhilePageLoads() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });

  // Create a search engine endpoint that will still
  // be loading resources on the page load.
  const SLOW_PAGE =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://www.example.com"
    ) + "slow-page.html";

  await SearchTestUtils.installSearchExtension({
    name: "Slow Engine",
    search_url: SLOW_PAGE,
    search_url_get_params: "search={searchTerms}",
  });
  await setDefaultEngine("Slow Engine");

  let engine = Services.search.getEngineByName("Slow Engine");
  let [expectedSearchUrl] = UrlbarUtils.getSearchQueryUrl(engine, "chocolate");

  // Load a slow SERP.
  await checkTab(
    window,
    expectedSearchUrl,
    UrlbarProviderSearchTips.TIP_TYPE.PERSIST
  );

  // Clean up.
  await SpecialPowers.popPrefEnv();
  resetSearchTipsProvider();
});

// Search tips modify the userTypedValue of a tab. The next time
// the pageproxystate is updated, the existence of the userTypedValue
// can change the pageproxystate. In the case of the Persist Search Tip,
// we don't want to change the pageproxystate while the Urlbar is non-focused,
// so check that when an event causes the pageproxystate to update
// (e.g. a SERP pushing state), the pageproxystate remains the same.
add_task(async function persistSearchTipAfterPushState() {
  await setDefaultEngine("Example");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: SEARCH_SERP_URL,
  });

  // Ensure the search tip is visible.
  await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.PERSIST, false);
  Assert.equal(
    gURLBar.getAttribute("pageproxystate"),
    "valid",
    "Urlbar is should be in a valid pageproxystate."
  );

  // Mock the default SERP using the History API on an exising website.
  // This is to trigger another call to setURI.
  await SpecialPowers.spawn(tab.linkedBrowser, [SEARCH_SERP_URL], async url => {
    content.history.pushState({}, "", url);
  });

  Assert.equal(
    gURLBar.getAttribute("pageproxystate"),
    "valid",
    "Urlbar is should be in a valid pageproxystate."
  );

  // Clean up.
  await SpecialPowers.popPrefEnv();
  resetSearchTipsProvider();
  BrowserTestUtils.removeTab(tab);
});

// Ensure a the Persist Search Tip is non-visible when a PopupNotification
// is already visible.
add_task(async function persistSearchTipBeforePopupShown() {
  await setDefaultEngine("Example");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: SEARCH_SERP_URL,
  });

  let promisePopupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  PopupNotifications.show(
    gBrowser.selectedBrowser,
    "test-notification",
    "This is a sample popup.",
    "geo-notification-icon"
  );
  await promisePopupShown;

  // Wait roughly for the amount of time it would take for the
  // persist search tip to show.
  await new Promise(resolve =>
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, UrlbarProviderSearchTips.SHOW_PERSIST_TIP_DELAY_MS * 2)
  );

  // Check the search tip didn't show while the page was loading.
  Assert.equal(
    UrlbarPrefs.get(
      `tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.PERSIST}`
    ),
    0,
    "The shownCount pref should be 0."
  );
  Assert.equal(false, window.gURLBar.view.isOpen, "Urlbar should be closed.");

  // Clean up.
  await SpecialPowers.popPrefEnv();
  resetSearchTipsProvider();
  BrowserTestUtils.removeTab(tab);
});

// The Persist Search Tip should be hidden when a PopupNotification appears.
add_task(async function persistSearchTipAfterPopupShown() {
  await setDefaultEngine("Example");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: SEARCH_SERP_URL,
  });

  // Ensure the search tip is visible.
  await checkTip(window, UrlbarProviderSearchTips.TIP_TYPE.PERSIST, false);

  // Show a popup after the search tip is shown.
  let promisePopupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  PopupNotifications.show(
    gBrowser.selectedBrowser,
    "test-notification",
    "This is a sample popup.",
    "geo-notification-icon"
  );
  await promisePopupShown;

  // The search tip should not be visible.
  Assert.equal(false, window.gURLBar.view.isOpen, "Urlbar should be closed.");
  Assert.equal(
    gURLBar.getAttribute("pageproxystate"),
    "valid",
    "Urlbar is should be in a valid pageproxystate."
  );

  // Clean up.
  await SpecialPowers.popPrefEnv();
  resetSearchTipsProvider();
  BrowserTestUtils.removeTab(tab);
});

function waitForBrowserWindowActive(win) {
  return new Promise(resolve => {
    if (Services.focus.activeWindow == win) {
      resolve();
    } else {
      win.addEventListener(
        "activate",
        () => {
          resolve();
        },
        { once: true }
      );
    }
  });
}
