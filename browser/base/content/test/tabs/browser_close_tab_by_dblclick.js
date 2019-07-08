/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PREF_CLOSE_TAB_BY_DBLCLICK = "browser.tabs.closeTabByDblclick";

function triggerDblclickOn(target) {
  let promise = BrowserTestUtils.waitForEvent(target, "dblclick");
  EventUtils.synthesizeMouseAtCenter(target, { clickCount: 1 });
  EventUtils.synthesizeMouseAtCenter(target, { clickCount: 2 });
  return promise;
}

add_task(async function dblclick() {
  let tab = gBrowser.selectedTab;
  await triggerDblclickOn(tab);
  ok(!tab.closing, "Double click the selected tab won't close it");
});

add_task(async function dblclickWithPrefSet() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_CLOSE_TAB_BY_DBLCLICK, true]],
  });

  let tab = BrowserTestUtils.addTab(gBrowser, "about:mozilla", {
    skipAnimation: true,
  });
  isnot(tab, gBrowser.selectedTab, "The new tab is in the background");

  await triggerDblclickOn(tab);
  is(tab, gBrowser.selectedTab, "Double click a background tab will select it");

  await triggerDblclickOn(tab);
  ok(tab.closing, "Double click the selected tab will close it");
});
