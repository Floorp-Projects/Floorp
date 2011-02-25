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
 * The Original Code is a test for bug 604098.
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

let originalTab;
let orphanedTab;
let contentWindow;

function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.show();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  contentWindow = document.getElementById("tab-view").contentWindow;
  originalTab = gBrowser.visibleTabs[0];

  test1();
}

function test1() {
  is(contentWindow.GroupItems.getOrphanedTabs().length, 0, "No orphaned tabs");

  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);

    let onTabViewShown = function() {
      window.removeEventListener("tabviewshown", onTabViewShown, false);

      is(contentWindow.GroupItems.getOrphanedTabs().length, 1, 
         "An orphaned tab is created");
      orphanedTab = contentWindow.GroupItems.getOrphanedTabs()[0].tab;

      test2();
    };
    window.addEventListener("tabviewshown", onTabViewShown, false);
    TabView.show();
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  // first click
  EventUtils.sendMouseEvent(
    { type: "mousedown" }, contentWindow.document.getElementById("content"), 
    contentWindow);
  EventUtils.sendMouseEvent(
    { type: "mouseup" }, contentWindow.document.getElementById("content"), 
    contentWindow);
  // second click
  EventUtils.sendMouseEvent(
    { type: "mousedown" }, contentWindow.document.getElementById("content"), 
    contentWindow);
  EventUtils.sendMouseEvent(
    { type: "mouseup" }, contentWindow.document.getElementById("content"), 
    contentWindow);
}

function test2() {
  let groupItem = createEmptyGroupItem(contentWindow, 300, 300, 200);
  is(groupItem.getChildren().length, 0, "The group is empty");

  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);

    is(groupItem.getChildren().length, 1, "A tab is created inside the group");
    
    gBrowser.selectedTab = originalTab;
    gBrowser.removeTab(orphanedTab);
    gBrowser.removeTab(groupItem.getChildren()[0].tab);

    finish();
  };
  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  // first click
  EventUtils.sendMouseEvent(
    { type: "mousedown" }, groupItem.container, contentWindow);
  EventUtils.sendMouseEvent(
    { type: "mouseup" }, groupItem.container, contentWindow);
  // second click
  EventUtils.sendMouseEvent(
    { type: "mousedown" }, groupItem.container, contentWindow);
  EventUtils.sendMouseEvent(
    { type: "mouseup" }, groupItem.container, contentWindow);
}
