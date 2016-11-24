"use strict";

const TEST_URL = "data:text/html,a test page";
const DRAG_URL = "http://www.example.com/";

add_task(function* checkURLBarUpdateForDrag() {
  yield BrowserTestUtils.withNewTab(TEST_URL, function* (browser) {
    EventUtils.synthesizeDrop(browser, gURLBar, [[{type: "text/uri-list", data: DRAG_URL}]], "copy", window);
    is(gURLBar.value, TEST_URL, "URL bar value should not have changed");
    is(gBrowser.selectedBrowser.userTypedValue, null, "Stored URL bar value should not have changed");
  });
});
