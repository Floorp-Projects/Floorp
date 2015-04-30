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
  gBrowser.selectedTab = gBrowser.addTab(URL);
  let browser = gBrowser.selectedBrowser;
  yield BrowserTestUtils.browserLoaded(browser);

  is((yield getFocusedLocalName(browser)), "button", "button is focused");

  let promiseFocused = ContentTask.spawn(browser, null, function* () {
    return new Promise(resolve => {
      content.addEventListener("focus", function onFocus({target}) {
        if (String(target.location).startsWith("data:")) {
          content.removeEventListener("focus", onFocus);
          resolve();
        }
      });
    });
  });

  // The test page loaded, so open an empty tab, select it, then restore
  // the test tab. This causes the test page's focused element to be removed
  // from its document.
  gBrowser.selectedTab = gBrowser.addTab("about:blank");
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.removeCurrentTab();

  // Wait until the original tab is focused again.
  yield promiseFocused;

  // Make sure focus is given to the window because the element is now gone.
  is((yield getFocusedLocalName(browser)), "body", "body is focused");

  // Cleanup.
  gBrowser.removeCurrentTab();
});
