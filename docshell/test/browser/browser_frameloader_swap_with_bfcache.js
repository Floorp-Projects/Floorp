/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
add_task(async function test() {
  if (
    !SpecialPowers.Services.appinfo.sessionHistoryInParent ||
    !SpecialPowers.Services.prefs.getBoolPref("fission.bfcacheInParent")
  ) {
    ok(
      true,
      "This test is currently only for the bfcache in the parent process."
    );
    return;
  }
  const uri =
    "data:text/html,<script>onpageshow = function(e) { document.documentElement.setAttribute('persisted', e.persisted); }; </script>";
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, uri, true);
  let browser1 = tab1.linkedBrowser;

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, uri, true);
  let browser2 = tab2.linkedBrowser;
  BrowserTestUtils.startLoadingURIString(browser2, uri + "nextpage");
  await BrowserTestUtils.browserLoaded(browser2, false);

  gBrowser.swapBrowsersAndCloseOther(tab1, tab2);
  is(tab1.linkedBrowser, browser1, "Tab's browser should stay the same.");
  browser1.goBack(false);
  await BrowserTestUtils.browserLoaded(browser1, false);
  let persisted = await SpecialPowers.spawn(browser1, [], async function () {
    return content.document.documentElement.getAttribute("persisted");
  });

  is(persisted, "false", "BFCache should be evicted when swapping browsers.");

  BrowserTestUtils.removeTab(tab1);
});
