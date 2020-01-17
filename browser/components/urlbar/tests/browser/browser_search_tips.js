/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the Search Tips feature, which displays a prompt to use the Urlbar on
// the newtab page and on the user's default search engine's homepage.
// Specifically, it tests that the Tips appear when they should be appearing.
// This doesn't test the max-shown-count limit because it requires restarting
// the browser.

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.jsm",
  HttpServer: "resource://testing-common/httpd.js",
  ProfileAge: "resource://gre/modules/ProfileAge.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProviderSearchTips: "resource:///modules/UrlbarProviderSearchTips.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
});

// These should match the same consts in UrlbarProviderSearchTips.jsm.
const SHOW_TIP_DELAY_MS = 200;
const MAX_SHOWN_COUNT = 4;
const LAST_UPDATE_THRESHOLD_MS = 24 * 60 * 60 * 1000;

const TIPS = {
  NONE: 0,
  ONBOARD: 1,
  REDIRECT: 2,
};

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
      ["browser.urlbar.update1.searchTips", true],
      ["browser.urlbar.searchTips.shownCount", 0],
      // We must disable our Top Sites behaviour for this test until we find a
      // way for multiple restricting providers to work together.
      // See bug 1607797.
      ["browser.urlbar.openViewOnFocus", false],
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

  registerCleanupFunction(async () => {
    let age2 = await ProfileAge();
    age2._times = originalTimes;
    await age2.writeTimes();
  });
});

// The onboarding tip should be shown on about:newtab.
add_task(async function newtab() {
  await checkTab(window, "about:newtab", TIPS.ONBOARD);
});

// The onboarding tip should be shown on about:home.
add_task(async function home() {
  await checkTab(window, "about:home", TIPS.ONBOARD);
});

// The redirect tip should be shown for www.google.com when it's the default
// engine.
add_task(async function google() {
  await setDefaultEngine("Google");
  for (let domain of GOOGLE_DOMAINS) {
    await withDNSRedirect(domain, "/", async url => {
      await checkTab(window, url, TIPS.REDIRECT);
    });
  }
});

// The redirect tip should be shown for www.google.com/webhp when it's the
// default engine.
add_task(async function googleWebhp() {
  await setDefaultEngine("Google");
  for (let domain of GOOGLE_DOMAINS) {
    await withDNSRedirect(domain, "/webhp", async url => {
      await checkTab(window, url, TIPS.REDIRECT);
    });
  }
});

// The redirect tip should be shown for the Google homepage when query strings
// are appended.
add_task(async function googleQueryString() {
  await setDefaultEngine("Google");
  for (let domain of GOOGLE_DOMAINS) {
    await withDNSRedirect(domain, "/webhp", async url => {
      await checkTab(window, `${url}?hl=en`, TIPS.REDIRECT);
    });
  }
});

// The redirect tip should not be shown on Google results pages.
add_task(async function googleResults() {
  await setDefaultEngine("Google");
  for (let domain of GOOGLE_DOMAINS) {
    await withDNSRedirect(domain, "/search", async url => {
      await checkTab(window, `${url}?q=firefox`, TIPS.NONE);
    });
  }
});

// The redirect tip should not be shown for www.google.com when it's not the
// default engine.
add_task(async function googleNotDefault() {
  await setDefaultEngine("Bing");
  for (let domain of GOOGLE_DOMAINS) {
    await withDNSRedirect(domain, "/", async url => {
      await checkTab(window, url, TIPS.NONE);
    });
  }
});

// The redirect tip should not be shown for www.google.com/webhp when it's not
// the default engine.
add_task(async function googleWebhpNotDefault() {
  await setDefaultEngine("Bing");
  for (let domain of GOOGLE_DOMAINS) {
    await withDNSRedirect(domain, "/webhp", async url => {
      await checkTab(window, url, TIPS.NONE);
    });
  }
});

// The redirect tip should be shown for www.bing.com when it's the default
// engine.
add_task(async function bing() {
  await setDefaultEngine("Bing");
  await withDNSRedirect("www.bing.com", "/", async url => {
    await checkTab(window, url, TIPS.REDIRECT);
  });
});

// The redirect tip should be shown on the Bing homepage even when Bing appends
// query strings.
add_task(async function bingQueryString() {
  await setDefaultEngine("Bing");
  await withDNSRedirect("www.bing.com", "/", async url => {
    await checkTab(window, `${url}?toWww=1`, TIPS.REDIRECT);
  });
});

// The redirect tip should not be shown on Bing results pages.
add_task(async function bingResults() {
  await setDefaultEngine("Bing");
  await withDNSRedirect("www.bing.com", "/search", async url => {
    await checkTab(window, `${url}?q=firefox`, TIPS.NONE);
  });
});

// The redirect tip should not be shown for www.bing.com when it's not the
// default engine.
add_task(async function bingNotDefault() {
  await setDefaultEngine("Google");
  await withDNSRedirect("www.bing.com", "/", async url => {
    await checkTab(window, url, TIPS.NONE);
  });
});

// The redirect tip should be shown for duckduckgo.com when it's the default
// engine.
add_task(async function ddg() {
  await setDefaultEngine("DuckDuckGo");
  await withDNSRedirect("duckduckgo.com", "/", async url => {
    await checkTab(window, url, TIPS.REDIRECT);
  });
});

// The redirect tip should be shown for start.duckduckgo.com when it's the
// default engine.
add_task(async function ddgStart() {
  await setDefaultEngine("DuckDuckGo");
  await withDNSRedirect("start.duckduckgo.com", "/", async url => {
    await checkTab(window, url, TIPS.REDIRECT);
  });
});

// The redirect tip should not be shown for duckduckgo.com when it's not the
// default engine.
add_task(async function ddgNotDefault() {
  await setDefaultEngine("Google");
  await withDNSRedirect("duckduckgo.com", "/", async url => {
    await checkTab(window, url, TIPS.NONE);
  });
});

// The redirect tip should not be shown for start.duckduckgo.com when it's not
// the default engine.
add_task(async function ddgStartNotDefault() {
  await setDefaultEngine("Google");
  await withDNSRedirect("start.duckduckgo.com", "/", async url => {
    await checkTab(window, url, TIPS.NONE);
  });
});

// The redirect tip should not be shown for duckduckgo.com/?q=foo, the search
// results page, which happens to have the same domain and path as the home
// page.
add_task(async function ddgSearchResultsPage() {
  await setDefaultEngine("DuckDuckGo");
  await withDNSRedirect("duckduckgo.com", "/", async url => {
    await checkTab(window, `${url}?q=test`, TIPS.NONE);
  });
});

// The redirect tip should not be shown on a non-engine page.
add_task(async function nonEnginePage() {
  await checkTab(window, "http://example.com/", TIPS.NONE);
});

// Tips should be shown at most once per session.
add_task(async function oncePerSession() {
  await setDefaultEngine("Google");
  await checkTab(window, "about:newtab", TIPS.ONBOARD, false);
  await checkTab(window, "about:newtab", TIPS.NONE, false);
  await withDNSRedirect("www.google.com", "/", async url => {
    await checkTab(window, url, TIPS.NONE);
  });
});

// Picking the tip's button should cause the Urlbar to blank out and the tip to
// be not to be shown again in any session.
add_task(async function pickButton() {
  UrlbarProviderSearchTips.showedTipInCurrentSession = false;
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:newtab",
    waitForLoad: false,
  });
  await checkTip(window, TIPS.ONBOARD, false);

  // Click the tip button.  The extension should send the engaged message.
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  let button = result.element.row._elements.get("tipButton");
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeMouseAtCenter(button, {});
  });

  Assert.equal(
    UrlbarPrefs.get("searchTips.shownCount"),
    MAX_SHOWN_COUNT,
    "Provider is disabled after tip button is picked."
  );
  Assert.equal(gURLBar.value, "", "The Urlbar should be empty.");
  resetProvider();

  BrowserTestUtils.removeTab(tab);
});

// When a tip is shown and the user engages with the urlbar, the tip should not
// be shown again in any session.
add_task(async function engage() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:newtab",
    waitForLoad: false,
  });
  await checkTip(window, TIPS.ONBOARD, false);

  // Do a search and press enter.  The extension should send the engaged
  // message.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "example.com",
    waitForFocus,
    fireInputEvent: true,
  });
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Enter");
  });

  Assert.equal(
    UrlbarPrefs.get("searchTips.shownCount"),
    MAX_SHOWN_COUNT,
    "Provider is disabled after engagement."
  );
  resetProvider();

  BrowserTestUtils.removeTab(tab);
});

// The tip shouldn't be shown when there's another notification present.
add_task(async function notification() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let box = gBrowser.getNotificationBox();
    let note = box.appendNotification(
      "Test",
      "urlbar-test",
      null,
      box.PRIORITY_INFO_HIGH,
      null,
      null,
      null
    );
    // Give it a big persistence so it doesn't go away on page load.
    note.persistence = 100;
    await withDNSRedirect("www.google.com", "/", async url => {
      await BrowserTestUtils.loadURI(gBrowser.selectedBrowser, url);
      await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
      await checkTip(window, TIPS.NONE);
      box.removeNotification(note, true);
    });
  });
  resetProvider();
});

// The tip should be shown when switching to a tab where it should be shown.
add_task(async function tabSwitch() {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:newtab");
  UrlbarProviderSearchTips.showedTipInCurrentSession = false;
  await BrowserTestUtils.switchTab(gBrowser, tab);
  await checkTip(window, TIPS.ONBOARD);
  BrowserTestUtils.removeTab(tab);
  resetProvider();
});

async function checkTip(win, expectedTip, closeView = true) {
  if (!expectedTip) {
    // Wait a bit for the tip to not show up.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 3 * SHOW_TIP_DELAY_MS));
    Assert.ok(!win.gURLBar.view.isOpen);
    return;
  }

  // Wait for the view to open, and then check the tip result.
  await UrlbarTestUtils.promisePopupOpen(win, () => {});
  Assert.ok(true, "View opened");
  Assert.equal(UrlbarTestUtils.getResultCount(win), 1);
  let result = await UrlbarTestUtils.getDetailsOfResultAt(win, 0);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.TIP);
  let heuristic;
  let title;
  let name = Services.search.defaultEngine.name;
  switch (expectedTip) {
    case TIPS.ONBOARD:
      heuristic = true;
      title =
        `Type less, find more: Search ${name} right from your ` +
        `address bar.`;
      break;
    case TIPS.REDIRECT:
      heuristic = false;
      title =
        `Start your search here to see suggestions from ${name} ` +
        `and your browsing history.`;
      break;
  }
  Assert.equal(result.heuristic, heuristic);
  Assert.equal(result.displayed.title, title);
  Assert.equal(
    result.element.row._elements.get("tipButton").textContent,
    `Okay, Got It`
  );
  Assert.ok(
    BrowserTestUtils.is_hidden(result.element.row._elements.get("helpButton"))
  );

  if (closeView) {
    await UrlbarTestUtils.promisePopupClose(win);
  }
}

async function checkTab(win, url, expectedTip, reset = true) {
  // BrowserTestUtils.withNewTab always waits for tab load, which hangs on
  // about:newtab for some reason, so don't use it.
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    url,
    waitForLoad: url != "about:newtab",
  });

  await checkTip(win, expectedTip, true);
  if (reset) {
    resetProvider();
  }

  BrowserTestUtils.removeTab(tab);
}

/**
 * This lets us visit www.google.com (for example) and have it redirect to
 * our test HTTP server instead of visiting the actual site.
 * @param {string} domain
 *   The domain to which we are redirecting.
 * @param {string} path
 *   The pathname on the domain.
 * @param {function} callback
 *   Executed when the test suite thinks `domain` is loaded.
 */
async function withDNSRedirect(domain, path, callback) {
  // Some domains have special security requirements, like www.bing.com.  We
  // need to override them to successfully load them.  This part is adapted from
  // testing/marionette/cert.js.
  const certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);
  Services.prefs.setBoolPref(
    "network.stricttransportsecurity.preloadlist",
    false
  );
  Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 0);
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  // Now set network.dns.localDomains to redirect the domain to localhost and
  // set up an HTTP server.
  Services.prefs.setCharPref("network.dns.localDomains", domain);

  let server = new HttpServer();
  server.registerPathHandler(path, (req, resp) => {
    resp.write(`Test! http://${domain}${path}`);
  });
  server.start(-1);
  server.identity.setPrimary("http", domain, server.identity.primaryPort);
  let url = `http://${domain}:${server.identity.primaryPort}${path}`;

  await callback(url);

  // Reset network.dns.localDomains and stop the server.
  Services.prefs.clearUserPref("network.dns.localDomains");
  await new Promise(resolve => server.stop(resolve));

  // Reset the security stuff.
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
  Services.prefs.clearUserPref("network.stricttransportsecurity.preloadlist");
  Services.prefs.clearUserPref("security.cert_pinning.enforcement_level");
  const sss = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  sss.clearAll();
  sss.clearPreloads();
}

function resetProvider() {
  Services.prefs.setIntPref("browser.urlbar.searchTips.shownCount", 0);
  UrlbarProviderSearchTips.showedTipInCurrentSession = false;
}

async function setDefaultEngine(name) {
  let engine = (await Services.search.getEngines()).find(e => e.name == name);
  Assert.ok(engine);
  await Services.search.setDefault(engine);
}
