"use strict";

const TEST_URL = "data:text/html,a test page";
const DRAG_URL = "http://www.example.com/";
const DRAG_FORBIDDEN_URL = "chrome://browser/content/aboutDialog.xul";
const DRAG_TEXT = "Firefox is awesome";
const DRAG_TEXT_URL = "http://example.com/?q=Firefox+is+awesome";
const DRAG_WORD = "Firefox";
const DRAG_WORD_URL = "http://example.com/?q=Firefox";

registerCleanupFunction(async function cleanup() {
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  }
  Services.search.defaultEngine = originalEngine;
  let engine = Services.search.getEngineByName("MozSearch");
  Services.search.removeEngine(engine);
});

let originalEngine;
add_task(async function test_setup() {
  // Stop search-engine loads from hitting the network
  Services.search.addEngineWithDetails("MozSearch", "", "", "", "GET",
                                       "http://example.com/?q={searchTerms}");
  let engine = Services.search.getEngineByName("MozSearch");
  originalEngine = Services.search.defaultEngine;
  Services.search.defaultEngine = engine;
});

add_task(async function checkDragURL() {
  await BrowserTestUtils.withNewTab(TEST_URL, function(browser) {
    // Have to use something other than the URL bar as a source, so picking the
    // home button somewhat arbitrarily:
    EventUtils.synthesizeDrop(document.getElementById("home-button"), gURLBar,
                              [[{type: "text/plain", data: DRAG_URL}]], "copy", window);
    is(gURLBar.value, TEST_URL, "URL bar value should not have changed");
    is(gBrowser.selectedBrowser.userTypedValue, null, "Stored URL bar value should not have changed");
  });
});

add_task(async function checkDragForbiddenURL() {
  await BrowserTestUtils.withNewTab(TEST_URL, function(browser) {
    EventUtils.synthesizeDrop(document.getElementById("home-button"), gURLBar,
                              [[{type: "text/plain", data: DRAG_FORBIDDEN_URL}]], "copy", window);
    isnot(gURLBar.value, DRAG_FORBIDDEN_URL, "Shouldn't be allowed to drop forbidden URL on URL bar");
  });
});

add_task(async function checkDragText() {
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    let promiseLoad = BrowserTestUtils.browserLoaded(browser, false, DRAG_TEXT_URL);
    EventUtils.synthesizeDrop(document.getElementById("home-button"), gURLBar,
                              [[{type: "text/plain", data: DRAG_TEXT}]], "copy", window);
    await promiseLoad;

    promiseLoad = BrowserTestUtils.browserLoaded(browser, false, DRAG_WORD_URL);
    EventUtils.synthesizeDrop(document.getElementById("home-button"), gURLBar,
                              [[{type: "text/plain", data: DRAG_WORD}]], "copy", window);
    await promiseLoad;
  });
});
