/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_hiding_tooltip() {
  let page1 = "data:text/html,<html title='title'><body>page 1<body></html>";
  let page2 = "data:text/html,<html><body>page 2</body></html>";

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: page1,
  });

  let popup = new Promise(function (resolve) {
    window.addEventListener("popupshown", resolve, { once: true });
  });
  // Fire a mousemove to trigger the tooltip.
  EventUtils.synthesizeMouseAtCenter(gBrowser.selectedBrowser, {
    type: "mousemove",
  });
  await popup;

  let hiding = new Promise(function (resolve) {
    window.addEventListener("popuphiding", resolve, { once: true });
  });
  let loaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    page2
  );
  BrowserTestUtils.startLoadingURIString(gBrowser, page2);
  await loaded;
  await hiding;

  ok(true, "Should have hidden the tooltip");
  BrowserTestUtils.removeTab(tab);
});
