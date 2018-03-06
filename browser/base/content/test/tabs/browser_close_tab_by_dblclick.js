/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const PREF_CLOSE_TAB_BY_DBLCLICK = "browser.tabs.closeTabByDblclick";

function triggerDblclickOn(target) {
  EventUtils.synthesizeMouseAtCenter(target, { clickCount: 1 });
  EventUtils.synthesizeMouseAtCenter(target, { clickCount: 2 });
}

add_task(async function dblclick() {
  let tab = gBrowser.selectedTab;

  let promise = BrowserTestUtils.waitForEvent(tab, "dblclick");
  triggerDblclickOn(tab);
  await promise;
  ok(!tab.closing, "Double click the selected tab won't close it");
});

add_task(async function dblclickWithPrefSet() {
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_CLOSE_TAB_BY_DBLCLICK, true]
  ]});

  let promise = BrowserTestUtils.waitForNewTab(gBrowser, "about:mozilla");
  BrowserTestUtils.addTab(gBrowser, "about:mozilla");
  let tab = await promise;
  isnot(tab, gBrowser.selectedTab, "The new tab is in the background");

  promise = BrowserTestUtils.waitForEvent(tab, "dblclick");
  triggerDblclickOn(tab);
  await promise;
  is(tab, gBrowser.selectedTab, "Double click a background tab will select it");

  promise = BrowserTestUtils.waitForEvent(tab, "dblclick");
  triggerDblclickOn(tab);
  await promise;
  ok(tab.closing, "Double click the selected tab will close it");
});
