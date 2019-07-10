/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { UrlbarTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlbarTestUtils.jsm"
);

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
  is(
    tab.currentURI.spec,
    expectedUrl,
    `Clicking ${elementId} opened ${expectedUrl} in the same tab.`
  );
}

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
  return (
    win.gURLBar.hasAttribute("focused") &&
    win.gURLBar.textbox.classList.contains("hidden-focus")
  );
}

function urlBarHasNormalFocus(win) {
  return (
    win.gURLBar.hasAttribute("focused") &&
    !win.gURLBar.textbox.classList.contains("hidden-focus")
  );
}

/**
 * Tests the search hand-off on character keydown in "about:privatebrowsing".
 */
add_task(async function test_search_handoff_on_keydown() {
  let { win, tab } = await openAboutPrivateBrowsing();

  await ContentTask.spawn(tab, null, async function() {
    let btn = content.document.getElementById("search-handoff-button");
    btn.click();
    ok(btn.classList.contains("focused"), "in-content search has focus styles");
  });
  ok(urlBarHasHiddenFocus(win), "url bar has hidden focused");
  await new Promise(r => EventUtils.synthesizeKey("f", {}, win, r));
  await ContentTask.spawn(tab, null, async function() {
    ok(
      content.document
        .getElementById("search-handoff-button")
        .classList.contains("hidden"),
      "in-content search is hidden"
    );
  });
  ok(urlBarHasNormalFocus(win), "url bar has normal focused");
  is(win.gURLBar.value, "@google f", "url bar has search text");
  await UrlbarTestUtils.promiseSearchComplete(win);
  // Close the popup.
  await UrlbarTestUtils.promisePopupClose(win);

  // Hitting ESC should reshow the in-content search
  await new Promise(r => EventUtils.synthesizeKey("KEY_Escape", {}, win, r));
  await ContentTask.spawn(tab, null, async function() {
    ok(
      !content.document
        .getElementById("search-handoff-button")
        .classList.contains("hidden"),
      "in-content search is not hidden"
    );
  });

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Tests the search hand-off on composition start in "about:privatebrowsing".
 */
add_task(async function test_search_handoff_on_composition_start() {
  let { win, tab } = await openAboutPrivateBrowsing();

  await ContentTask.spawn(tab, null, async function() {
    content.document.getElementById("search-handoff-button").click();
  });
  ok(urlBarHasHiddenFocus(win), "url bar has hidden focused");
  await new Promise(r =>
    EventUtils.synthesizeComposition({ type: "compositionstart" }, win, r)
  );
  ok(urlBarHasNormalFocus(win), "url bar has normal focused");

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Tests the search hand-off on paste in "about:privatebrowsing".
 */
add_task(async function test_search_handoff_on_paste() {
  let { win, tab } = await openAboutPrivateBrowsing();

  await ContentTask.spawn(tab, null, async function() {
    content.document.getElementById("search-handoff-button").click();
  });
  ok(urlBarHasHiddenFocus(win), "url bar has hidden focused");
  var helper = SpecialPowers.Cc[
    "@mozilla.org/widget/clipboardhelper;1"
  ].getService(SpecialPowers.Ci.nsIClipboardHelper);
  helper.copyString("words");
  await new Promise(r =>
    EventUtils.synthesizeKey("v", { accelKey: true }, win, r)
  );

  await UrlbarTestUtils.promiseSearchComplete(win);

  ok(urlBarHasNormalFocus(win), "url bar has normal focused");
  is(win.gURLBar.value, "@google words", "url bar has search text");

  await BrowserTestUtils.closeWindow(win);
});
