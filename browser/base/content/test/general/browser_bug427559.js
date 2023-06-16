"use strict";

/*
 * Test bug 427559 to make sure focused elements that are no longer on the page
 * will have focus transferred to the window when changing tabs back to that
 * tab with the now-gone element.
 */

// Default focus on a button and have it kill itself on blur.
const URL =
  "data:text/html;charset=utf-8," +
  '<body><button onblur="this.remove()">' +
  "<script>document.body.firstElementChild.focus()</script></body>";

function getFocusedLocalName(browser) {
  return SpecialPowers.spawn(browser, [], async function () {
    return content.document.activeElement.localName;
  });
}

add_task(async function () {
  let testTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  let browser = testTab.linkedBrowser;

  is(await getFocusedLocalName(browser), "button", "button is focused");

  let blankTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await BrowserTestUtils.switchTab(gBrowser, testTab);

  // Make sure focus is given to the window because the element is now gone.
  is(await getFocusedLocalName(browser), "body", "body is focused");

  // Cleanup.
  gBrowser.removeTab(blankTab);
  gBrowser.removeCurrentTab();
});
