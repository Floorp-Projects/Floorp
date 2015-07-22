"use strict";

/*
 * Test bug 427559 to make sure focused elements that are no longer on the page
 * will have focus transferred to the window when changing tabs back to that
 * tab with the now-gone element.
 */

// Default focus on a button and have it kill itself on blur.
const URL = 'data:text/html;charset=utf-8,' +
            '<body><button onblur="this.remove()">' +
            '<script>document.body.firstChild.focus()</script></body>';

function getFocusedLocalName(browser) {
  return ContentTask.spawn(browser, null, function* () {
    return content.document.activeElement.localName;
  });
}

add_task(function* () {
  let testTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  let browser = testTab.linkedBrowser;

  is((yield getFocusedLocalName(browser)), "button", "button is focused");

  let blankTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  yield BrowserTestUtils.switchTab(gBrowser, testTab);

  // Make sure focus is given to the window because the element is now gone.
  is((yield getFocusedLocalName(browser)), "body", "body is focused");

  // Cleanup.
  gBrowser.removeTab(blankTab);
  gBrowser.removeCurrentTab();

});
