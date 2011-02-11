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
 * The Original Code is Panorama expander test.
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
  requestLongerTimeout(2);
  newWindowWithTabView(onTabViewWindowLoaded);
}

function onTabViewWindowLoaded(win) {
  ok(win.TabView.isVisible(), "Tab View is visible");

  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  let [originalTab] = win.gBrowser.visibleTabs;
  let currentGroup = contentWindow.GroupItems.getActiveGroupItem();
  
  // let's create a group small enough to get stacked
  let group = new contentWindow.GroupItem([], {
    immediately: true,
    bounds: {left: 20, top: 300, width: 300, height: 300}
  });

  let expander = contentWindow.iQ(group.container).find(".stackExpander");
  ok("length" in expander && expander.length == 1, "The group has an expander.");

  // procreate!
  contentWindow.GroupItems.setActiveGroupItem(group);
  for (var i=0; i<7; i++) {
    win.gBrowser.loadOneTab('about:blank#' + i, {inBackground: true});
  }
  let children = group.getChildren();
  
  // Wait until they all update because, once updated, they will notice that they
  // don't have favicons and this will change their styling at some unknown time.
  afterAllTabItemsUpdated(function() {
    
    ok(!group.shouldStack(group._children.length), "The group should not stack.");
    is(expander[0].style.display, "none", "The expander is hidden.");
    
    // now resize the group down.
    group.setSize(100, 100, true);
  
    ok(group.shouldStack(group._children.length), "The group should stack.");
    isnot(expander[0].style.display, "none", "The expander is now visible!");
    let expanderBounds = expander.bounds();
    ok(group.getBounds().contains(expanderBounds), "The expander lies in the group.");
    let stackCenter = children[0].getBounds().center();
    ok(stackCenter.y < expanderBounds.center().y, "The expander is below the stack.");
  
    // STAGE 1:
    // Here, we just expand the group, click elsewhere, and make sure
    // it collapsed.
    let stage1expanded = function() {
      group.removeSubscriber("test stage 1", "expanded", stage1expanded);
    
      ok(group.expanded, "The group is now expanded.");
      is(expander[0].style.display, "none", "The expander is hidden!");
      
      let overlay = contentWindow.document.getElementById("expandedTray");    
      ok(overlay, "The expanded tray exists.");
      let $overlay = contentWindow.iQ(overlay);
      
      group.addSubscriber("test stage 1", "collapsed", stage1collapsed);
      // null type means "click", for some reason...
      EventUtils.synthesizeMouse(contentWindow.document.body, 10, $overlay.bounds().bottom + 5,
                                 {type: null}, contentWindow);
    };
    
    let stage1collapsed = function() {
      group.removeSubscriber("test stage 1", "collapsed", stage1collapsed);
      ok(!group.expanded, "The group is no longer expanded.");
      isnot(expander[0].style.display, "none", "The expander is visible!");
      let expanderBounds = expander.bounds();
      ok(group.getBounds().contains(expanderBounds), "The expander still lies in the group.");
      let stackCenter = children[0].getBounds().center();
      ok(stackCenter.y < expanderBounds.center().y, "The expander is below the stack.");
  
      // now, try opening it up again.
      group.addSubscriber("test stage 2", "expanded", stage2expanded);
      EventUtils.sendMouseEvent({ type: "click" }, expander[0], contentWindow);
    };
  
    // STAGE 2:
    // Now make sure every child of the group shows up within this "tray", and
    // click on one of them and make sure we go into the tab and the tray collapses.
    let stage2expanded = function() {
      group.removeSubscriber("test stage 2", "expanded", stage2expanded);
    
      ok(group.expanded, "The group is now expanded.");
      is(expander[0].style.display, "none", "The expander is hidden!");
      
      let overlay = contentWindow.document.getElementById("expandedTray");    
      ok(overlay, "The expanded tray exists.");
      let $overlay = contentWindow.iQ(overlay);
      let overlayBounds = $overlay.bounds();
  
      children.forEach(function(child, i) {
        ok(overlayBounds.contains(child.getBounds()), "Child " + i + " is in the overlay");
      });
      
      win.addEventListener("tabviewhidden", stage2hidden, false);
      // again, null type means "click", for some reason...
      EventUtils.synthesizeMouse(children[1].container, 2, 2, {type: null}, contentWindow);
    };
  
    let stage2hidden = function() {
      win.removeEventListener("tabviewhidden", stage2hidden, false);
      
      is(win.gBrowser.selectedTab, children[1].tab, "We clicked on the second child.");
      
      win.addEventListener("tabviewshown", stage2shown, false);
      win.TabView.toggle();
    };
    
    let stage2shown = function() {
      win.removeEventListener("tabviewshown", stage2shown, false);
      ok(!group.expanded, "The group is not expanded.");
      isnot(expander[0].style.display, "none", "The expander is visible!");
      let expanderBounds = expander.bounds();
      ok(group.getBounds().contains(expanderBounds), "The expander still lies in the group.");
      let stackCenter = children[0].getBounds().center();
      ok(stackCenter.y < expanderBounds.center().y, "The expander is below the stack.");

      is(group.topChild, children[1], "The top child in the stack is the second tab item");
      let topChildzIndex = children[1].zIndex;
      // the second tab item should have the largest z-index.
      // only check the first 6 tabs as the stack only contains 6 tab items.
      for (let i = 0; i < 6; i++) {
        if (i != 1)
          ok(children[i].zIndex < topChildzIndex,
            "The child[" + i + "] has smaller zIndex than second dhild");
      }

      // okay, expand this group one last time
      group.addSubscriber("test stage 3", "expanded", stage3expanded);
      EventUtils.sendMouseEvent({ type: "click" }, expander[0], contentWindow);
    }

    // STAGE 3:
    // Ensure that stack still shows the same top item after a expand and a collapse.
    let stage3expanded = function() {
      group.removeSubscriber("test stage 3", "expanded", stage3expanded);

      ok(group.expanded, "The group is now expanded.");
      let overlay = contentWindow.document.getElementById("expandedTray");    
      let $overlay = contentWindow.iQ(overlay);

      group.addSubscriber("test stage 3", "collapsed", stage3collapsed);
      // null type means "click", for some reason...
      EventUtils.synthesizeMouse(contentWindow.document.body, 10, $overlay.bounds().bottom + 5,
                                 {type: null}, contentWindow);
    };

    let stage3collapsed = function() {
      group.removeSubscriber("test stage 3", "collapsed", stage3collapsed);

      ok(!group.expanded, "The group is no longer expanded.");
      isnot(expander[0].style.display, "none", "The expander is visible!");

      let stackCenter = children[0].getBounds().center();
      ok(stackCenter.y < expanderBounds.center().y, "The expander is below the stack.");

      is(group.topChild, children[1], 
         "The top child in the stack is still the second tab item");
      let topChildzIndex = children[1].zIndex;
      // the second tab item should have the largest z-index.
      // only check the first 6 tabs as the stack only contains 6 tab items.
      for (let i = 0; i < 6; i++) {
        if (i != 1)
          ok(children[i].zIndex < topChildzIndex,
            "The child[" + i + "] has smaller zIndex than second dhild after a collapse.");
      }

      // In preparation for Stage 4, find that original tab and make it the active tab.
      let originalTabItem = originalTab._tabViewTabItem;
      contentWindow.UI.setActiveTab(originalTabItem);

      // now, try opening it up again.
      group.addSubscriber("test stage 4", "expanded", stage4expanded);
      EventUtils.sendMouseEvent({ type: "click" }, expander[0], contentWindow);
    };

    // STAGE 4:
    // Activate another tab not in this group, expand our stacked group, but then
    // enter Panorama (i.e., zoom into this other group), and make sure we can go to
    // it and that the tray gets collapsed.
    let stage4expanded = function() {
      group.removeSubscriber("test stage 4", "expanded", stage4expanded);
    
      ok(group.expanded, "The group is now expanded.");
      is(expander[0].style.display, "none", "The expander is hidden!");
      let overlay = contentWindow.document.getElementById("expandedTray");
      ok(overlay, "The expanded tray exists.");
      
      let activeTab = contentWindow.UI.getActiveTab();
      ok(activeTab, "There is an active tab.");
      let originalTabItem = originalTab._tabViewTabItem;

      isnot(activeTab, originalTabItem, "But it's not what it was a moment ago.");
      let someChildIsActive = group.getChildren().some(function(child)
                              child == activeTab);
      ok(someChildIsActive, "Now one of the children in the group is active.");
            
      // now activate Panorama...
      win.addEventListener("tabviewhidden", stage4hidden, false);
      win.TabView.toggle();
    };
  
    let stage4hidden = function() {
      win.removeEventListener("tabviewhidden", stage4hidden, false);
      
      isnot(win.gBrowser.selectedTab, originalTab, "We did not enter the original tab.");

      let someChildIsSelected = group.getChildren().some(function(child)
                                  child.tab == win.gBrowser.selectedTab);
      ok(someChildIsSelected, "Instead we're in one of the stack's children.");
      
      win.addEventListener("tabviewshown", stage4shown, false);
      win.TabView.toggle();
    };
    
    let stage4shown = function() {
      win.removeEventListener("tabviewshown", stage4shown, false);
  
      let overlay = contentWindow.document.getElementById("expandedTray");
      ok(!group.expanded, "The group is no longer expanded.");
      isnot(expander[0].style.display, "none", "The expander is visible!");

      win.close();
      finish();
    }
  
    // get the ball rolling
    group.addSubscriber("test stage 1", "expanded", stage1expanded);
    EventUtils.sendMouseEvent({ type: "click" }, expander[0], contentWindow);
  }, win);
}
