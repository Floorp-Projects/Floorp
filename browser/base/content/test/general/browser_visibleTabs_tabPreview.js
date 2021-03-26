/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ctrlTab.sortByRecentlyUsed", true]],
  });

  let [origTab] = gBrowser.visibleTabs;
  let tabOne = BrowserTestUtils.addTab(gBrowser);
  let tabTwo = BrowserTestUtils.addTab(gBrowser);

  // test the ctrlTab.tabList
  pressCtrlTab();
  ok(ctrlTab.isOpen, "With 3 tab open, Ctrl+Tab opens the preview panel");
  is(ctrlTab.tabList.length, 3, "Ctrl+Tab panel displays all visible tabs");
  releaseCtrl();

  gBrowser.showOnlyTheseTabs([origTab]);
  pressCtrlTab();
  ok(
    !ctrlTab.isOpen,
    "With 1 tab open, Ctrl+Tab doesn't open the preview panel"
  );
  releaseCtrl();

  gBrowser.showOnlyTheseTabs([origTab, tabOne, tabTwo]);
  pressCtrlTab();
  ok(
    ctrlTab.isOpen,
    "Ctrl+Tab opens the preview panel after re-showing hidden tabs"
  );
  is(
    ctrlTab.tabList.length,
    3,
    "Ctrl+Tab panel displays all visible tabs after re-showing hidden ones"
  );
  releaseCtrl();

  // cleanup
  gBrowser.removeTab(tabOne);
  gBrowser.removeTab(tabTwo);
});

function pressCtrlTab(aShiftKey) {
  EventUtils.synthesizeKey("VK_TAB", { ctrlKey: true, shiftKey: !!aShiftKey });
}

function releaseCtrl() {
  EventUtils.synthesizeKey("VK_CONTROL", { type: "keyup" });
}
