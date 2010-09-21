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
 * The Original Code is tabview bug 959943 test.
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

  // Show TabView
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
  ok(contentWindow.GroupItems.groupItems[0]._children[0].tab == originalTab, 
    "the original tab is in the original group");
      
  // create a second group 
  let box = new contentWindow.Rect(20, 20, 180, 180);
  let groupItem = new contentWindow.GroupItem([], { bounds: box });
  is(contentWindow.GroupItems.groupItems.length, 2, "we now have two groups");
  contentWindow.GroupItems.setActiveGroupItem(groupItem);
  
  // create a second tab
  let normalXulTab = gBrowser.loadOneTab("about:blank");
  is(gBrowser.tabs.length, 2, "we now have two tabs");
  is(groupItem._children.length, 1, "the new tab was added to the group");
  
  // create a third tab
  let appXulTab = gBrowser.loadOneTab("about:blank");
  is(gBrowser.tabs.length, 3, "we now have three tabs");
  gBrowser.pinTab(appXulTab);
  is(groupItem._children.length, 1, "the app tab is not in the group");
  
  // We now have two groups with one tab each, plus an app tab.
  // Click into one of the tabs, close it and make sure we don't go back to Tab View.
  function onTabViewHidden() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);
    ok(!TabView.isVisible(), "Tab View is hidden because we clicked on the app tab");
  
    // Remove the tab we're looking at. Note: this will also close groupItem (verified below)
    gBrowser.removeTab(normalXulTab);  
  
    // Make sure we haven't returned to TabView; this is the crux of this test
    ok(!TabView.isVisible(), "Tab View remains hidden");
  
    // clean up
    gBrowser.selectedTab = originalTab;
    
    gBrowser.unpinTab(appXulTab);
    gBrowser.removeTab(appXulTab);

    // Verify ending state
    is(gBrowser.tabs.length, 1, "we finish with one tab");
    is(contentWindow.GroupItems.groupItems.length, 1, "we finish with one group");
    ok(!TabView.isVisible(), "we finish with Tab View hidden");
      
    finish();
  }

  window.addEventListener("tabviewhidden", onTabViewHidden, false);
  EventUtils.sendMouseEvent({ type: "mousedown" }, normalXulTab.tabItem.container, contentWindow);
  EventUtils.sendMouseEvent({ type: "mouseup" }, normalXulTab.tabItem.container, contentWindow);
}