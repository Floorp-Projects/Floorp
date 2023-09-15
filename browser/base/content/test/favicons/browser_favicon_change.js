/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT =
  "http://mochi.test:8888/browser/browser/base/content/test/favicons/";
const TEST_URL = TEST_ROOT + "file_favicon_change.html";

add_task(async function () {
  let extraTab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));
  let haveChanged = waitForFavicon(
    extraTab.linkedBrowser,
    TEST_ROOT + "file_bug970276_favicon1.ico"
  );

  BrowserTestUtils.startLoadingURIString(extraTab.linkedBrowser, TEST_URL);
  await BrowserTestUtils.browserLoaded(extraTab.linkedBrowser);
  await haveChanged;

  haveChanged = waitForFavicon(extraTab.linkedBrowser, TEST_ROOT + "moz.png");

  SpecialPowers.spawn(extraTab.linkedBrowser, [], function () {
    let ev = new content.CustomEvent("PleaseChangeFavicon", {});
    content.dispatchEvent(ev);
  });

  await haveChanged;

  ok(true, "Saw all the icons we expected.");

  gBrowser.removeTab(extraTab);
});
