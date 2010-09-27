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
 * The Original Code is tabview layout test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
 * Sean Dunn <seanedunn@yahoo.com>
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

  // verify initial state
  ok(!TabView.isVisible(), "Tab View starts hidden");

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

let originalGroupItem = null;
let originalTab = null;
let contentWindow = null;

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(TabView.isVisible(), "Tab View is visible");

  contentWindow = document.getElementById("tab-view").contentWindow;

  is(contentWindow.GroupItems.groupItems.length, 1, "There is one group item on startup");
  originalGroupItem = contentWindow.GroupItems.groupItems[0];
  is(originalGroupItem.getChildren().length, 1, "There should be one Tab Item in that group.");
  contentWindow.GroupItems.setActiveGroupItem(originalGroupItem);

  [originalTab] = gBrowser.visibleTabs;

  testEmptyGroupItem(contentWindow);
}

function testEmptyGroupItem(contentWindow) {
  let groupItemCount = contentWindow.GroupItems.groupItems.length;

  // Preparation
  //
    
  // create empty group item
  let emptyGroupItem = createEmptyGroupItem(contentWindow, 253, 335, 100, true);
  ok(emptyGroupItem.isEmpty(), "This group is empty");

  is(contentWindow.GroupItems.groupItems.length, ++groupItemCount,
     "The number of groups is increased by 1");

  // add four blank items
  contentWindow.GroupItems.setActiveGroupItem(emptyGroupItem);

  let numNewTabs = 4;
  let items = [];
  for(let t=0; t<numNewTabs; t++) {
    let newItem = contentWindow.gBrowser.loadOneTab("about:blank")._tabViewTabItem;
    ok(newItem.container, "Created element "+t+":"+newItem.container);
    items.push(newItem);
  }

  // Define main test function
  //

  let mainTestFunc = function() {
    for(let j=0; j<numNewTabs; j++) {
      for(let i=0; i<numNewTabs; i++) {
        if (j!=i) {
          // make sure there is no overlap between j's title and i's box.
          let jitem = items[j];
          let iitem = items[i];
          let $jtitle = contentWindow.iQ(jitem.container).find(".tab-title");
          let jbounds = $jtitle.bounds();
          let ibounds = contentWindow.iQ(iitem.container).bounds();

          ok(
            (jbounds.top+jbounds.height < ibounds.top) || 
            (jbounds.top > ibounds.top + ibounds.height) ||
            (jbounds.left+jbounds.width < ibounds.left) || 
            (jbounds.left > ibounds.left + ibounds.width),
            "Items do not overlap: "
            +jbounds.left+","+jbounds.top+","+jbounds.width+","+jbounds.height+" ; "
            +ibounds.left+","+ibounds.top+","+ibounds.width+","+ibounds.height);        
        }
      }
    }

    // Shut down
    emptyGroupItem.addSubscriber(emptyGroupItem, "close", function() {
      emptyGroupItem.removeSubscriber(emptyGroupItem, "close");
  
      // check the number of groups.
      is(contentWindow.GroupItems.groupItems.length, --groupItemCount,
         "The number of groups is decreased by 1");

      let onTabViewHidden = function() {
        window.removeEventListener("tabviewhidden", onTabViewHidden, false);
        // assert that we're no longer in tab view
        ok(!TabView.isVisible(), "Tab View is hidden");
        finish();
      };
      window.addEventListener("tabviewhidden", onTabViewHidden, false);
  
      TabView.toggle();
    });
  
    let closeButton = emptyGroupItem.container.getElementsByClassName("close");
    ok(closeButton[0], "Group close button exists");
  
    // click the close button
    EventUtils.synthesizeMouse(closeButton[0], 1, 1, {}, contentWindow);
  };

  mainTestFunc();
}
