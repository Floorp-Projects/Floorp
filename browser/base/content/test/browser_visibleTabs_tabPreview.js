/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is tab preview with tab view.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function test() {
  gPrefService.setBoolPref("browser.ctrlTab.previews", true);

  let [origTab] = gBrowser.visibleTabs;
  let tabOne = gBrowser.addTab();
  let tabTwo = gBrowser.addTab();

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

  if (gPrefService.prefHasUserValue("browser.ctrlTab.previews"))
    gPrefService.clearUserPref("browser.ctrlTab.previews");
}

function pressCtrlTab(aShiftKey) {
  EventUtils.synthesizeKey("VK_TAB", { ctrlKey: true, shiftKey: !!aShiftKey });
}

function releaseCtrl() {
  EventUtils.synthesizeKey("VK_CONTROL", { type: "keyup" });
}
