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
 * The Original Code is tabview test for bug 587040.
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
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(TabView.isVisible(), "Tab View is visible");

  let [originalTab] = gBrowser.visibleTabs;
  let contentWindow = document.getElementById("tab-view").contentWindow;

  // create group which we'll close
  let box1 = new contentWindow.Rect(310, 10, 300, 300);
  let group1 = new contentWindow.GroupItem([], { bounds: box1 });
  ok(group1.isEmpty(), "This group is empty");
  contentWindow.GroupItems.setActiveGroupItem(group1);
  let tab1 = gBrowser.loadOneTab("about:blank#1", {inBackground: true});
  let tab1Item = tab1.tabItem;
  ok(group1.getChildren().some(function(child) child == tab1Item), "The tab was made in our new group");
  is(group1.getChildren().length, 1, "Only one tab in the first group");

  group1.addSubscriber(group1, "close", function() {
    group1.removeSubscriber(group1, "close");

    let onTabViewHidden = function() {
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
      // assert that we're no longer in tab view
      ok(!TabView.isVisible(), "Tab View is hidden");
      finish();
    };
    window.addEventListener("tabviewhidden", onTabViewHidden, false);

    EventUtils.synthesizeKey("e", {accelKey : true}, contentWindow);
  });

  // Get rid of the group and its children
  group1.closeAll();
  
  // close undo group
  let closeButton = group1.$undoContainer.find(".close");
  EventUtils.sendMouseEvent(
    { type: "click" }, closeButton[0], contentWindow);
}

