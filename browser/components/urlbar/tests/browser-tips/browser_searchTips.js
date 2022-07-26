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
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderSearchTips:
    "resource:///modules/UrlbarProviderSearchTips.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.jsm",
  HttpServer: "resource://testing-common/httpd.js",
  ProfileAge: "resource://gre/modules/ProfileAge.jsm",
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

add_task(async function init() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        `browser.urlbar.tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.ONBOARD}`,
        0,
      ],
      [
        `browser.urlbar.tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.REDIRECT}`,
        0,
      ],
    ],
  });

  // Write an old profile age so tips are actually shown.
  let age = await ProfileAge();
  let originalTimes = age._times;
  let date = Date.now() - LAST_UPDATE_THRESHOLD_MS - 30000;
  age._times = { created: date, firstUse: date };
  await age.writeTimes();

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

  registerCleanupFunction(async () => {
    let age2 = await ProfileAge();
    age2._times = originalTimes;
    await age2.writeTimes();
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
