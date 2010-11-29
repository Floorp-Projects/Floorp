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
 * The Original Code is tabview bug 586553 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
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

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

let moves = 0;
let contentWindow = null;
let newTabs = [];
let originalTab = null;

function onTabMove(e) {
  let tab = e.target;
  moves++;
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  
  contentWindow = document.getElementById("tab-view").contentWindow;
  
  originalTab = gBrowser.selectedTab;
  newTabs = [gBrowser.addTab("about:robots"), gBrowser.addTab("about:mozilla"), gBrowser.addTab("about:credits")];

  is(originalTab._tPos, 0, "Original tab is in position 0");
  is(newTabs[0]._tPos, 1, "Robots is in position 1");
  is(newTabs[1]._tPos, 2, "Mozilla is in position 2");
  is(newTabs[2]._tPos, 3, "Credits is in position 3");
  
  gBrowser.tabContainer.addEventListener("TabMove", onTabMove, false);
    
  groupItem = contentWindow.GroupItems.getActiveGroupItem();
  
  // move 3 > 0 (and therefore 0 > 1, 1 > 2, 2 > 3)
  groupItem._children.splice(0, 0, groupItem._children.splice(3, 1)[0]);
  groupItem.arrange();
  
  window.addEventListener("tabviewhidden", onTabViewWindowHidden, false);
  TabView.toggle();
}

function onTabViewWindowHidden() {
  window.removeEventListener("tabviewhidden", onTabViewWindowHidden, false);
  gBrowser.tabContainer.removeEventListener("TabMove", onTabMove, false);
  
  is(moves, 1, "Only one move should be necessary for this basic move.");

  is(originalTab._tPos, 1, "Original tab is in position 0");
  is(newTabs[0]._tPos, 2, "Robots is in position 1");
  is(newTabs[1]._tPos, 3, "Mozilla is in position 2");
  is(newTabs[2]._tPos, 0, "Credits is in position 3");
  
  gBrowser.removeTab(newTabs[0]);
  gBrowser.removeTab(newTabs[1]);
  gBrowser.removeTab(newTabs[2]);
  finish();
}
