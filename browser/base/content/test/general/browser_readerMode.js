/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that the reader mode button appears and works properly on
 * reader-able content, and that ReadingList button can open and close
 * its Sidebar UI.
 */
const READER_PREF = "reader.parse-on-load.enabled";
const READING_LIST_PREF = "browser.readinglist.enabled";

const TEST_PATH = "http://example.com/browser/browser/base/content/test/general/";

let readerButton = document.getElementById("reader-mode-button");

add_task(function* () {
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(READER_PREF);
    Services.prefs.clearUserPref(READING_LIST_PREF);
    while (gBrowser.tabs.length > 1) {
      gBrowser.removeCurrentTab();
    }
  });

  // Enable the reader mode and ReadingList buttons.
  Services.prefs.setBoolPref(READER_PREF, true);
  Services.prefs.setBoolPref(READING_LIST_PREF, true);

  let tab = gBrowser.selectedTab = gBrowser.addTab();
  is_element_hidden(readerButton, "Reader mode button is not present on a new tab");

  // Point tab to a test page that is reader-able.
  let url = TEST_PATH + "readerModeArticle.html";
  yield promiseTabLoadEvent(tab, url);
  yield promiseWaitForCondition(() => !readerButton.hidden);
  is_element_visible(readerButton, "Reader mode button is present on a reader-able page");

  readerButton.click();
  yield promiseTabLoadEvent(tab);

  let readerUrl = gBrowser.selectedBrowser.currentURI.spec;
  ok(readerUrl.startsWith("about:reader"), "about:reader loaded after clicking reader mode button");
  is_element_visible(readerButton, "Reader mode button is present on about:reader");

  is(gURLBar.value, readerUrl, "gURLBar value is about:reader URL");
  is(gURLBar.textValue, url.substring("http://".length), "gURLBar is displaying original article URL");

  // Readinglist button should be present, and status should be "closed".
  let listButton;
  yield promiseWaitForCondition(() =>
    listButton = gBrowser.contentDocument.getElementById("list-button"));
  is_element_visible(listButton, "List button is present on a reader-able page");
  yield promiseWaitForCondition(() => !listButton.classList.contains("on"));
  ok(!listButton.classList.contains("on"),
    "List button should not indicate SideBar-ReadingList open.");
  ok(!ReadingListUI.isSidebarOpen,
    "The ReadingListUI should not indicate SideBar-ReadingList open.");

  // After we click ReadingList button, status should be "open".
  listButton.click();
  yield promiseWaitForCondition(() => listButton.classList.contains("on"));
  ok(listButton.classList.contains("on"),
    "List button should now indicate SideBar-ReadingList open.");
  ok(ReadingListUI.isSidebarOpen,
    "The ReadingListUI should now indicate SideBar-ReadingList open.");

  readerButton.click();
  yield promiseTabLoadEvent(tab);
  is(gBrowser.selectedBrowser.currentURI.spec, url, "Original page loaded after clicking active reader mode button");

  // Load a new tab that is NOT reader-able.
  let newTab = gBrowser.selectedTab = gBrowser.addTab();
  yield promiseTabLoadEvent(newTab, "about:robots");
  yield promiseWaitForCondition(() => readerButton.hidden);
  is_element_hidden(readerButton, "Reader mode button is not present on a non-reader-able page");

  // Switch back to the original tab to make sure reader mode button is still visible.
  gBrowser.removeCurrentTab();
  yield promiseWaitForCondition(() => !readerButton.hidden);
  is_element_visible(readerButton, "Reader mode button is present on a reader-able page");
});
