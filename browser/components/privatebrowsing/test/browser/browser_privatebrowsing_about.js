/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.import(
    "resource://testing-common/UrlbarTestUtils.jsm"
  );
  module.init(this);
  return module;
});

/**
 * Clicks the given link and checks this opens the given URI in the same tab.
 *
 * This function does not return to the previous page.
 */
async function testLinkOpensUrl({ win, tab, elementId, expectedUrl }) {
  let loadedPromise = BrowserTestUtils.browserLoaded(tab);
  await SpecialPowers.spawn(tab, [elementId], async function(elemId) {
    content.document.getElementById(elemId).click();
  });
  await loadedPromise;
  is(
    tab.currentURI.spec,
    expectedUrl,
    `Clicking ${elementId} opened ${expectedUrl} in the same tab.`
  );
}

let expectedEngineAlias;
let expectedIconURL;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault", true],
      // Enable suggestions in this test. Otherwise, the behaviour of the
      // content search box changes.
      ["browser.search.suggest.enabled", true],
    ],
  });

  const originalPrivateDefault = await Services.search.getDefaultPrivate();
  // We have to use a built-in engine as we are currently hard-coding the aliases.
  const privateEngine = await Services.search.getEngineByName("DuckDuckGo");
  await Services.search.setDefaultPrivate(privateEngine);
  expectedEngineAlias = privateEngine.aliases[0];
  expectedIconURL = privateEngine.iconURI.spec;

  registerCleanupFunction(async () => {
    await Services.search.setDefaultPrivate(originalPrivateDefault);
  });
});

/**
 * Tests the private-browsing-myths link in "about:privatebrowsing".
 */
add_task(async function test_myths_link() {
  Services.prefs.setCharPref("app.support.baseURL", "https://example.com/");
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("app.support.baseURL");
  });

  let { win, tab } = await openAboutPrivateBrowsing();

  await testLinkOpensUrl({
    win,
    tab,
    elementId: "private-browsing-myths",
    expectedUrl: "https://example.com/private-browsing-myths",
  });

  await BrowserTestUtils.closeWindow(win);
});

function urlBarHasHiddenFocus(win) {
  return win.gURLBar.focused && !win.gURLBar.hasAttribute("focused");
}

function urlBarHasNormalFocus(win) {
  return win.gURLBar.hasAttribute("focused");
}

/**
 * Tests that we have the correct icon displayed.
 */
add_task(async function test_search_icon() {
  let { win, tab } = await openAboutPrivateBrowsing();

  await SpecialPowers.spawn(tab, [expectedIconURL], async function(iconURL) {
    is(
      content.document.body.getAttribute("style"),
      `--newtab-search-icon: url(${iconURL});`,
      "Should have the correct icon URL for the logo"
    );
  });

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Tests the search hand-off on character keydown in "about:privatebrowsing".
 */
add_task(async function test_search_handoff_on_keydown() {
  let { win, tab } = await openAboutPrivateBrowsing();

  await SpecialPowers.spawn(tab, [], async function() {
    let btn = content.document.getElementById("search-handoff-button");
    btn.click();
    ok(btn.classList.contains("focused"), "in-content search has focus styles");
  });
  ok(urlBarHasHiddenFocus(win), "Urlbar has hidden focus");

  // Expect two searches, one to enter search mode and then another in search
  // mode.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(win);

  await new Promise(r => EventUtils.synthesizeKey("f", {}, win, r));
  await SpecialPowers.spawn(tab, [], async function() {
    ok(
      content.document
        .getElementById("search-handoff-button")
        .classList.contains("disabled"),
      "in-content search is disabled"
    );
  });
  await searchPromise;
  ok(urlBarHasNormalFocus(win), "Urlbar has normal focus");
  is(win.gURLBar.value, "f", "url bar has search text");

  // Close the popup.
  await UrlbarTestUtils.promisePopupClose(win);

  // Hitting ESC should reshow the in-content search
  await new Promise(r => EventUtils.synthesizeKey("KEY_Escape", {}, win, r));
  await SpecialPowers.spawn(tab, [], async function() {
    ok(
      !content.document
        .getElementById("search-handoff-button")
        .classList.contains("disabled"),
      "in-content search is not disabled"
    );
  });

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Tests the search hand-off on composition start in "about:privatebrowsing".
 */
add_task(async function test_search_handoff_on_composition_start() {
  let { win, tab } = await openAboutPrivateBrowsing();

  await SpecialPowers.spawn(tab, [], async function() {
    content.document.getElementById("search-handoff-button").click();
  });
  ok(urlBarHasHiddenFocus(win), "Urlbar has hidden focus");
  await new Promise(r =>
    EventUtils.synthesizeComposition({ type: "compositionstart" }, win, r)
  );
  ok(urlBarHasNormalFocus(win), "Urlbar has normal focus");

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Tests the search hand-off on paste in "about:privatebrowsing".
 */
add_task(async function test_search_handoff_on_paste() {
  let { win, tab } = await openAboutPrivateBrowsing();

  await SpecialPowers.spawn(tab, [], async function() {
    content.document.getElementById("search-handoff-button").click();
  });
  ok(urlBarHasHiddenFocus(win), "Urlbar has hidden focus");
  var helper = SpecialPowers.Cc[
    "@mozilla.org/widget/clipboardhelper;1"
  ].getService(SpecialPowers.Ci.nsIClipboardHelper);
  helper.copyString("words");

  // Expect two searches, one to enter search mode and then another in search
  // mode.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(win);

  await new Promise(r =>
    EventUtils.synthesizeKey("v", { accelKey: true }, win, r)
  );

  await searchPromise;

  ok(urlBarHasNormalFocus(win), "Urlbar has normal focus");
  is(win.gURLBar.value, "words", "Urlbar has search text");

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Tests that handoff enters search mode when suggestions are disabled.
 */
add_task(async function test_search_handoff_search_mode() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", false]],
  });

  let { win, tab } = await openAboutPrivateBrowsing();

  await SpecialPowers.spawn(tab, [], async function() {
    let btn = content.document.getElementById("search-handoff-button");
    btn.click();
    ok(btn.classList.contains("focused"), "in-content search has focus styles");
  });
  ok(urlBarHasHiddenFocus(win), "Urlbar has hidden focus");

  // Expect two searches, one to enter search mode and then another in search
  // mode.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(win);

  await new Promise(r => EventUtils.synthesizeKey("f", {}, win, r));
  await SpecialPowers.spawn(tab, [], async function() {
    ok(
      content.document
        .getElementById("search-handoff-button")
        .classList.contains("disabled"),
      "in-content search is disabled"
    );
  });
  await searchPromise;
  ok(urlBarHasNormalFocus(win), "Urlbar has normal focus");
  await UrlbarTestUtils.assertSearchMode(win, {
    engineName: "DuckDuckGo",
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    entry: "handoff",
  });
  is(win.gURLBar.value, "f", "url bar has search text");

  // Close the popup.
  await UrlbarTestUtils.exitSearchMode(win);
  await UrlbarTestUtils.promisePopupClose(win);

  // Hitting ESC should reshow the in-content search
  await new Promise(r => EventUtils.synthesizeKey("KEY_Escape", {}, win, r));
  await SpecialPowers.spawn(tab, [], async function() {
    ok(
      !content.document
        .getElementById("search-handoff-button")
        .classList.contains("disabled"),
      "in-content search is not disabled"
    );
  });

  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});
