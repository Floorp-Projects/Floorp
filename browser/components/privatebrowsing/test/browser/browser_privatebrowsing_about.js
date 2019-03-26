/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TP_PB_ENABLED_PREF = "privacy.trackingprotection.pbmode.enabled";

const {UrlbarTestUtils} = ChromeUtils.import("resource://testing-common/UrlbarTestUtils.jsm");

/**
 * Opens a new private window and loads "about:privatebrowsing" there.
 */
async function openAboutPrivateBrowsing() {
  let win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
    waitForTabURL: "about:privatebrowsing",
  });
  let tab = win.gBrowser.selectedBrowser;
  return { win, tab };
}

/**
 * Clicks the given link and checks this opens a new tab with the given URI.
 */
async function testLinkOpensTab({ win, tab, elementId, expectedUrl }) {
  let newTabPromise = BrowserTestUtils.waitForNewTab(win.gBrowser, expectedUrl);
  await ContentTask.spawn(tab, elementId, async function(elemId) {
    content.document.getElementById(elemId).click();
  });
  let newTab = await newTabPromise;
  ok(true, `Clicking ${elementId} opened ${expectedUrl} in a new tab.`);
  BrowserTestUtils.removeTab(newTab);
}

/**
 * Clicks the given link and checks this opens the given URI in the same tab.
 *
 * This function does not return to the previous page.
 */
async function testLinkOpensUrl({ win, tab, elementId, expectedUrl }) {
  let loadedPromise = BrowserTestUtils.browserLoaded(tab);
  await ContentTask.spawn(tab, elementId, async function(elemId) {
    content.document.getElementById(elemId).click();
  });
  await loadedPromise;
  is(tab.currentURI.spec, expectedUrl,
     `Clicking ${elementId} opened ${expectedUrl} in the same tab.`);
}

/**
 * Enables the searchUI pref.
 */
function enableSearchUI() {
  Services.prefs.setBoolPref("browser.privatebrowsing.searchUI", true);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.privatebrowsing.searchUI");
  });
}

/**
 * Tests the links in "about:privatebrowsing".
 */
add_task(async function test_links() {
  // Use full version and change the remote URLs to prevent network access.
  Services.prefs.setCharPref("app.support.baseURL", "https://example.com/");
  Services.prefs.setCharPref("privacy.trackingprotection.introURL",
                             "https://example.com/tour");
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("privacy.trackingprotection.introURL");
    Services.prefs.clearUserPref("app.support.baseURL");
  });

  let { win, tab } = await openAboutPrivateBrowsing();

  await testLinkOpensTab({ win, tab,
    elementId: "learnMore",
    expectedUrl: "https://example.com/private-browsing",
  });

  await testLinkOpensUrl({ win, tab,
    elementId: "startTour",
    expectedUrl: "https://example.com/tour?variation=1",
  });

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Tests the private-browsing-myths link in "about:privatebrowsing".
 */
add_task(async function test_myths_link() {
  enableSearchUI();
  Services.prefs.setCharPref("app.support.baseURL", "https://example.com/");
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("app.support.baseURL");
  });

  let { win, tab } = await openAboutPrivateBrowsing();

  await testLinkOpensUrl({ win, tab,
    elementId: "private-browsing-myths",
    expectedUrl: "https://example.com/private-browsing-myths",
  });

  await BrowserTestUtils.closeWindow(win);
});

function urlBarHasHiddenFocus(win) {
  return win.gURLBar.hasAttribute("focused") &&
    win.gURLBar.textbox.classList.contains("hidden-focus");
}

function urlBarHasNormalFocus(win) {
  return win.gURLBar.hasAttribute("focused") &&
    !win.gURLBar.textbox.classList.contains("hidden-focus");
}

/**
 * Tests the search hand-off on character keydown in "about:privatebrowsing".
 */
add_task(async function test_search_handoff_on_keydown() {
  enableSearchUI();
  let { win, tab } = await openAboutPrivateBrowsing();

  await ContentTask.spawn(tab, null, async function() {
    let btn = content.document.getElementById("search-handoff-button");
    btn.click();
    ok(btn.classList.contains("focused"), "in-content search has focus styles");
  });
  ok(urlBarHasHiddenFocus(win), "url bar has hidden focused");
  await new Promise(r => EventUtils.synthesizeKey("f", {}, win, r));
  await ContentTask.spawn(tab, null, async function() {
    ok(content.document.getElementById("search-handoff-button").classList.contains("hidden"),
      "in-content search is hidden");
  });
  ok(urlBarHasNormalFocus(win), "url bar has normal focused");
  is(win.gURLBar.value, "@google f", "url bar has search text");
  await UrlbarTestUtils.promiseSearchComplete(win);
  // Close the popup.
  await UrlbarTestUtils.promisePopupClose(win);

  // Hitting ESC should reshow the in-content search
  await new Promise(r => EventUtils.synthesizeKey("KEY_Escape", {}, win, r));
  await ContentTask.spawn(tab, null, async function() {
    ok(!content.document.getElementById("search-handoff-button").classList.contains("hidden"),
      "in-content search is not hidden");
  });

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Tests the search hand-off on composition start in "about:privatebrowsing".
 */
add_task(async function test_search_handoff_on_composition_start() {
  enableSearchUI();
  let { win, tab } = await openAboutPrivateBrowsing();

  await ContentTask.spawn(tab, null, async function() {
    content.document.getElementById("search-handoff-button").click();
  });
  ok(urlBarHasHiddenFocus(win), "url bar has hidden focused");
  await new Promise(r => EventUtils.synthesizeComposition({type: "compositionstart"}, win, r));
  ok(urlBarHasNormalFocus(win), "url bar has normal focused");

  await BrowserTestUtils.closeWindow(win);
});

/**
* Tests the search hand-off on paste in "about:privatebrowsing".
*/
add_task(async function test_search_handoff_on_paste() {
  enableSearchUI();
  let { win, tab } = await openAboutPrivateBrowsing();

  await ContentTask.spawn(tab, null, async function() {
    content.document.getElementById("search-handoff-button").click();
  });
  ok(urlBarHasHiddenFocus(win), "url bar has hidden focused");
  var helper = SpecialPowers.Cc["@mozilla.org/widget/clipboardhelper;1"]
     .getService(SpecialPowers.Ci.nsIClipboardHelper);
  helper.copyString("words");
  await new Promise(r => EventUtils.synthesizeKey("v", {accelKey: true}, win, r));
  // TODO: Bug 1539199 We should be able to wait for search complete for AwesomeBar
  // as well.
  if (UrlbarPrefs.get("quantumbar")) {
    await UrlbarTestUtils.promiseSearchComplete(win);
  }
  ok(urlBarHasNormalFocus(win), "url bar has normal focused");
  is(win.gURLBar.value, "@google words", "url bar has search text");

  await BrowserTestUtils.closeWindow(win);
});
