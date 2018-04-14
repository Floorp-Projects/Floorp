/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_URL = encodeURI(`data:text/html;charset=utf-8,<a id="foo" href="http://nic.xn--rhqv96g/">abc</a><span id="bar">def</span>`);
const TEST_STATUS_TEXT = "nic.\u4E16\u754C";

/**
 * Test that if the StatusPanel displays an IDN
 * (Bug 1450538).
 */
add_task(async function test_show_statuspanel_twice() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE_URL);

  let promise = promiseStatusPanelShown(window, TEST_STATUS_TEXT);
  ContentTask.spawn(tab.linkedBrowser, null, async () => {
    content.document.links[0].focus();
  });
  await promise;

  await BrowserTestUtils.removeTab(tab);
});
