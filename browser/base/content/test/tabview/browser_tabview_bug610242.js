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
 * The Original Code is bug 610242 test.
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
  win.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(win.TabView.isVisible(), "Tab View is visible");

  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  let [originalTab] = win.gBrowser.visibleTabs;

  let currentGroup = contentWindow.GroupItems.getActiveGroupItem();

  // Create a group and make it active
  let box = new contentWindow.Rect(100, 100, 370, 370);
  let group = new contentWindow.GroupItem([], { bounds: box });
  ok(group.isEmpty(), "This group is empty");
  contentWindow.GroupItems.setActiveGroupItem(group);
  is(contentWindow.GroupItems.getActiveGroupItem(), group, "new group is active");
  
  // Create a bunch of tabs in the group
  let bg = {inBackground: true};
  let datatext = win.gBrowser.loadOneTab("data:text/plain,bug610242", bg);
  let datahtml = win.gBrowser.loadOneTab("data:text/html,<blink>don't blink!</blink>", bg);
  let mozilla  = win.gBrowser.loadOneTab("about:mozilla", bg);
  let html     = win.gBrowser.loadOneTab("http://example.com", bg);
  let png      = win.gBrowser.loadOneTab("http://mochi.test:8888/browser/browser/base/content/test/moz.png", bg);
  let svg      = win.gBrowser.loadOneTab("http://mochi.test:8888/browser/browser/base/content/test/title_test.svg", bg);
  
  ok(!group.shouldStack(group._children.length), "Group should not stack.");
  
  // PREPARE FINISH:
  group.addSubscriber(group, "close", function() {
    group.removeSubscriber(group, "close");

    ok(group.isEmpty(), "The group is empty again");

    contentWindow.GroupItems.setActiveGroupItem(currentGroup);
    isnot(contentWindow.GroupItems.getActiveGroupItem(), null, "There is an active group");
    is(win.gBrowser.tabs.length, 1, "There is only one tab left");
    is(win.gBrowser.visibleTabs.length, 1, "There is also only one visible tab");

    let onTabViewHidden = function() {
      win.removeEventListener("tabviewhidden", onTabViewHidden, false);
      win.close();
      ok(win.closed, "new window is closed");
      finish();
    };
    win.addEventListener("tabviewhidden", onTabViewHidden, false);
    win.gBrowser.selectedTab = originalTab;

    win.TabView.hide();
  });

  function check(tab, label, visible) {
    let display = contentWindow.getComputedStyle(tab._tabViewTabItem.favEl, null).getPropertyValue("display");
    if (visible) {
      is(display, "block", label + " has favicon");
    } else {
      is(display, "none", label + " has no favicon");
    }
  }

  afterAllTabsLoaded(function() {
    afterAllTabItemsUpdated(function() {
      check(datatext, "datatext", false);
      check(datahtml, "datahtml", false);
      check(mozilla, "about:mozilla", false);
      check(html, "html", true);
      check(png, "png", false);
      check(svg, "svg", true);
  
      // Get rid of the group and its children
      // The group close will trigger a finish().
      group.addSubscriber(group, "groupHidden", function() {
        group.removeSubscriber(group, "groupHidden");
        group.closeHidden();
      });
      group.closeAll();
    }, win);  
  }, win);
}
