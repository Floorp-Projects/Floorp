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
 * The Original Code is Panorama bug 627736 (post-group close focus) test.
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
  newWindowWithTabView(onTabViewWindowLoaded);
}

function onTabViewWindowLoaded(win) {
  ok(win.TabView.isVisible(), "Tab View is visible");

  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  let [originalTab] = win.gBrowser.visibleTabs;
  let originalGroup = contentWindow.GroupItems.getActiveGroupItem();

  // open a group with a tab, and close either the group or the tab.
  function openAndClose(groupOrTab, callback) {
    // let's create a group with a tab.
    let group = new contentWindow.GroupItem([], {
      immediately: true,
      bounds: {left: 20, top: 20, width: 400, height: 400}
    });
    contentWindow.GroupItems.setActiveGroupItem(group);
    win.gBrowser.loadOneTab('about:blank', {inBackground: true});
  
    is(group.getChildren().length, 1, "The group has one child now.");
    let tab = group.getChild(0);
  
    function check() {
      if (groupOrTab == 'group') {
        group.removeSubscriber(group, "groupHidden", check);
        group.closeHidden();
      } else
        tab.removeSubscriber(tab, "tabRemoved", check);
  
      is(contentWindow.GroupItems.getActiveGroupItem(), originalGroup,
        "The original group is active.");
      is(contentWindow.UI.getActiveTab(), originalTab._tabViewTabItem,
        "The original tab is active");
  
      callback();
    }
  
    if (groupOrTab == 'group') {
      group.addSubscriber(group, "groupHidden", check);
      group.closeAll();
    } else {
      tab.addSubscriber(tab, "tabRemoved", check);
      tab.close();
    }
  }

  // PHASE 1: create a group with a tab and close the group.
  openAndClose("group", function postPhase1() {
    // PHASE 2: create a group with a tab and close the tab.
    openAndClose("tab", function postPhase2() {
      win.close();
      finish();
    });
  });
}