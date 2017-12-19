/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test() {
  Services.prefs.setBoolPref("browser.ctrlTab.previews", true);

  let [origTab] = gBrowser.visibleTabs;
  let tabOne = BrowserTestUtils.addTab(gBrowser);
  let tabTwo = BrowserTestUtils.addTab(gBrowser);

  // test the ctrlTab.tabList
  pressCtrlTab();
  ok(ctrlTab.tabList.length, 3, "Show 3 tabs in tab preview");
  releaseCtrl();

  gBrowser.showOnlyTheseTabs([origTab]);
  pressCtrlTab();
  ok(ctrlTab.tabList.length, 1, "Show 1 tab in tab preview");
  ok(!ctrlTab.isOpen, "With 1 tab open, Ctrl+Tab doesn't open the preview panel");

  gBrowser.showOnlyTheseTabs([origTab, tabOne, tabTwo]);
  pressCtrlTab();
  ok(ctrlTab.isOpen, "With 3 tabs open, Ctrl+Tab does open the preview panel");
  releaseCtrl();

  // cleanup
  gBrowser.removeTab(tabOne);
  gBrowser.removeTab(tabTwo);

  if (Services.prefs.prefHasUserValue("browser.ctrlTab.previews"))
    Services.prefs.clearUserPref("browser.ctrlTab.previews");
});

function pressCtrlTab(aShiftKey) {
  EventUtils.synthesizeKey("VK_TAB", { ctrlKey: true, shiftKey: !!aShiftKey });
}

function releaseCtrl() {
  EventUtils.synthesizeKey("VK_CONTROL", { type: "keyup" });
}
