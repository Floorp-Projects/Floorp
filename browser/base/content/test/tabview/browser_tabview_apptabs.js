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
 * The Original Code is tabview group test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
 * Ian Gilman <ian@iangilman.com>
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

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;

  // establish initial state
  is(contentWindow.GroupItems.groupItems.length, 1, "we start with one group (the default)"); 
  is(gBrowser.tabs.length, 1, "we start with one tab");
  let originalTab = gBrowser.tabs[0];
  
  // create a group 
  let box = new contentWindow.Rect(20, 20, 180, 180);
  let groupItemOne = new contentWindow.GroupItem([], { bounds: box, title: "test1" });
  is(contentWindow.GroupItems.groupItems.length, 2, "we now have two groups");
  contentWindow.GroupItems.setActiveGroupItem(groupItemOne);
  
  // create a tab
  let xulTab = gBrowser.loadOneTab("about:blank");
  is(gBrowser.tabs.length, 2, "we now have two tabs");
  is(groupItemOne._children.length, 1, "the new tab was added to the group");
  
  // make sure the group has no app tabs
  let appTabIcons = groupItemOne.container.getElementsByClassName("appTabIcon");
  is(appTabIcons.length, 0, "there are no app tab icons");
  
  // pin the tab, make sure the TabItem goes away and the icon comes on
  gBrowser.pinTab(xulTab);

  is(groupItemOne._children.length, 0, "the app tab's TabItem was removed from the group");

  appTabIcons = groupItemOne.container.getElementsByClassName("appTabIcon");
  is(appTabIcons.length, 1, "there's now one app tab icon");

  // create a second group and make sure it gets the icon too
  box.offset(box.width + 20, 0);
  let groupItemTwo = new contentWindow.GroupItem([], { bounds: box, title: "test2" });
  is(contentWindow.GroupItems.groupItems.length, 3, "we now have three groups");
  appTabIcons = groupItemTwo.container.getElementsByClassName("appTabIcon");
  is(appTabIcons.length, 1, "there's an app tab icon in the second group");
  
  // unpin the tab, make sure the icon goes away and the TabItem comes on
  gBrowser.unpinTab(xulTab);

  is(groupItemOne._children.length, 1, "the app tab's TabItem is back");

  appTabIcons = groupItemOne.container.getElementsByClassName("appTabIcon");
  is(appTabIcons.length, 0, "the icon is gone from group one");

  appTabIcons = groupItemTwo.container.getElementsByClassName("appTabIcon");
  is(appTabIcons.length, 0, "the icon is gone from group 2");
  
  // pin the tab again
  gBrowser.pinTab(xulTab);

  // close the second group
  groupItemTwo.close();

  // find app tab in group and hit it
  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);
    ok(!TabView.isVisible(), "Tab View is hidden because we clicked on the app tab");
    
    // clean up
    gBrowser.selectedTab = originalTab;
    
    gBrowser.unpinTab(xulTab);
    gBrowser.removeTab(xulTab);
    is(gBrowser.tabs.length, 1, "we finish with one tab");
  
    groupItemOne.close();
    is(contentWindow.GroupItems.groupItems.length, 1, "we finish with one group");
    
    ok(!TabView.isVisible(), "we finish with Tab View not visible");
    
    finish();
  };

  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  appTabIcons = groupItemOne.container.getElementsByClassName("appTabIcon");
  EventUtils.sendMouseEvent({ type: "click" }, appTabIcons[0], contentWindow);
}
