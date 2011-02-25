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
 * The Original Code is bug 597218 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Asaf Romano <mano@mozill.com>
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
  waitForExplicitFinish();

  function testState(aPinned) {
    function elemAttr(id, attr) document.getElementById(id).getAttribute(attr);

    if (aPinned) {
      is(elemAttr("key_close", "disabled"), "true",
         "key_close should be disabled when a pinned-tab is selected");
      is(elemAttr("menu_close", "key"), "",
         "menu_close shouldn't have a key set when a pinned is selected");
    }
    else {
      is(elemAttr("key_close", "disabled"), "",
         "key_closed shouldn't have disabled state set when a non-pinned tab is selected");
      is(elemAttr("menu_close", "key"), "key_close",
         "menu_close should have key_close set as its key when a non-pinned tab is selected");
    }
  }

  let lastSelectedTab = gBrowser.selectedTab;
  ok(!lastSelectedTab.pinned, "We should have started with a regular tab selected");

  testState(false);

  let pinnedTab = gBrowser.addTab("about:blank");
  gBrowser.pinTab(pinnedTab);

  // Just pinning the tab shouldn't change the key state.
  testState(false);

  // Test updating key state after selecting a tab.
  gBrowser.selectedTab = pinnedTab;
  testState(true);

  gBrowser.selectedTab = lastSelectedTab;
  testState(false);
  
  gBrowser.selectedTab = pinnedTab;
  testState(true);

  // Test updating the key state after un/pinning the tab.
  gBrowser.unpinTab(pinnedTab);
  testState(false);

  gBrowser.pinTab(pinnedTab);  
  testState(true);

  // Test updating the key state after removing the tab.
  gBrowser.removeTab(pinnedTab);
  testState(false);

  finish();
}
