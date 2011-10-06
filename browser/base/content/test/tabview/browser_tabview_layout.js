/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
  contentWindow.UI.setActive(originalGroupItem);

  [originalTab] = gBrowser.visibleTabs;

  testEmptyGroupItem(contentWindow);
}

function testEmptyGroupItem(contentWindow) {
  let groupItemCount = contentWindow.GroupItems.groupItems.length;

  // Preparation
  //
    
  // create empty group item
  let emptyGroupItem = createEmptyGroupItem(contentWindow, 253, 335, 100);
  ok(emptyGroupItem.isEmpty(), "This group is empty");

  is(contentWindow.GroupItems.groupItems.length, ++groupItemCount,
     "The number of groups is increased by 1");

  // add four blank items
  contentWindow.UI.setActive(emptyGroupItem);

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
    emptyGroupItem.addSubscriber("close", function onClose() {
      emptyGroupItem.removeSubscriber("close", onClose);
  
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
