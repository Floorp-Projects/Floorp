"use strict";

const TEST_URL = "data:text/html,a test page";
const DRAG_URL = "http://www.example.com/";
const DRAG_FORBIDDEN_URL = "chrome://browser/content/aboutDialog.xul";
const DRAG_TEXT = "Firefox is awesome";
const DRAG_WORD = "Firefox";

add_task(async function checkDragURL() {
  await BrowserTestUtils.withNewTab(TEST_URL, function(browser) {
    // Have to use something other than the URL bar as a source, so picking the
    // downloads button somewhat arbitrarily:
    EventUtils.synthesizeDrop(document.getElementById("downloads-button"), gURLBar,
                              [[{type: "text/plain", data: DRAG_URL}]], "copy", window);
    is(gURLBar.value, TEST_URL, "URL bar value should not have changed");
    is(gBrowser.selectedBrowser.userTypedValue, null, "Stored URL bar value should not have changed");
  });
});

add_task(async function checkDragForbiddenURL() {
  await BrowserTestUtils.withNewTab(TEST_URL, function(browser) {
    EventUtils.synthesizeDrop(document.getElementById("downloads-button"), gURLBar,
                              [[{type: "text/plain", data: DRAG_FORBIDDEN_URL}]], "copy", window);
    isnot(gURLBar.value, DRAG_FORBIDDEN_URL, "Shouldn't be allowed to drop forbidden URL on URL bar");
  });
});

add_task(async function checkDragText() {
  await BrowserTestUtils.withNewTab(TEST_URL, function(browser) {
    EventUtils.synthesizeDrop(document.getElementById("downloads-button"), gURLBar,
                              [[{type: "text/plain", data: DRAG_TEXT}]], "copy", window);
    is(gURLBar.value, DRAG_TEXT, "Dragging normal text should replace the URL bar value");

    EventUtils.synthesizeDrop(document.getElementById("downloads-button"), gURLBar,
                              [[{type: "text/plain", data: DRAG_WORD}]], "copy", window);
    is(gURLBar.value, DRAG_WORD, "Dragging a single word should replace the URL bar value");
  });
});
