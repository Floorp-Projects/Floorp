/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let prefsBranch;
let currentActiveGroupItemId;

function test() {
  waitForExplicitFinish();

  // disable the first run pref
  prefsBranch = Services.prefs.getBranch("browser.panorama.");
  prefsBranch.setBoolPref("experienced_first_run", false);

  let win =
    window.openDialog(getBrowserURL(), "_blank", "all,dialog=no", "about:blank");

  let onLoad = function() {
    win.removeEventListener("load", onLoad, false);

    let theObserver = function(subject, topic, data) {
      Services.obs.removeObserver(
        theObserver, "browser-delayed-startup-finished");
      test1(win);
    }
    Services.obs.addObserver(
      theObserver, "browser-delayed-startup-finished", false);
  };

  win.addEventListener("load", onLoad, false);
}

// check whether tha menu is hidden when the "experienced_first_run" pref is
// false
function test1(win) {
  is(win.gBrowser.tabs.length - win.gBrowser.visibleTabs.length, 0,
     "There are no hidden tabs")

  let originalTab = win.gBrowser.visibleTabs[0];

  openTabContextPopup(win, originalTab);

  ok(win.document.getElementById("context_tabViewMenu").hidden,
     "The menu should be hidden");

  closeTabContextPopup(win);

  executeSoon(function() { 
    ok(!win.TabView.getContentWindow(), "The tab view iframe is not loaded");
    test2(win);
  });
}

// press the next group key combination and check whether the iframe has loaded or not
function test2(win) {
  goToNextGroup(win);

  // give it a delay so we can ensure the iframe has not been loaded
  executeSoon(function() {
    ok(!win.TabView.getContentWindow(),
       "The tab view iframe is not loaded after pressing the next group key combination");

    test3(win);
  });
}

// first run has happened, open the tab context menupopup and the tabview menu,
// move a tab to another group including iframe initialization.  Then,
// use the key combination to change to the next group.
function test3(win) {
  prefsBranch.setBoolPref("experienced_first_run", true);

  let newTab = win.gBrowser.addTab("about:blank");

  // open the tab context menupopup and the tabview menupopup
  openTabContextPopup(win, newTab);
  win.document.getElementById("context_tabViewMenuPopup").openPopup(
    win.document.getElementById("context_tabViewMenu"), "end_after", 0, 0,
    true, false);

  ok(!win.document.getElementById("context_tabViewMenu").hidden,
     "The menu should be visible now");
  is(win.gBrowser.tabs.length - win.gBrowser.visibleTabs.length, 0,
     "There are no hidden tabs")
  ok(!win.TabView.getContentWindow(),
     "The tab view iframe is not loaded after opening tab context menu");

  let onTabViewFrameInitialized = function() {
     win.removeEventListener(
       "tabviewframeinitialized", onTabViewFrameInitialized, false);

     let contentWindow = win.document.getElementById("tab-view").contentWindow;

     // show the tab view to ensure groups are created before checking.
     let onTabViewShown = function() {
       win.removeEventListener("tabviewshown", onTabViewShown, false);

       ok(win.TabView.isVisible(), "Tab View is visible");

       is(contentWindow.GroupItems.groupItems.length, 2,
          "There are two group items");
       is(contentWindow.GroupItems.groupItems[0].getChildren().length, 1,
          "There should be one tab item in the first group");
       is(contentWindow.GroupItems.groupItems[1].getChildren().length, 1,
          "There should be one tab item in the second group");

       // simulate the next group key combination
       currentActiveGroupItemId =
         contentWindow.GroupItems.getActiveGroupItem().id;

       win.addEventListener("tabviewhidden", onTabViewHidden, false);

       win.TabView.toggle();
     };
     let onTabViewHidden = function() {
       win.removeEventListener("tabviewhidden", onTabViewHidden, false);

       goToNextGroup(win);

       isnot(contentWindow.GroupItems.getActiveGroupItem().id,
             currentActiveGroupItemId, "Just switched to another group");

       // close the window and finish it
       win.close();
       finish();
     };
     win.addEventListener("tabviewshown", onTabViewShown, false);
     // give it a delay to ensure everything is loaded.
     executeSoon(function() { win.TabView.toggle(); });
  };
  win.addEventListener(
    "tabviewframeinitialized", onTabViewFrameInitialized, false);
  // move the tab to another group using the menu item
  win.document.getElementById("context_tabViewNewGroup").doCommand();

  // close popups
  win.document.getElementById("context_tabViewMenuPopup").hidePopup();
  closeTabContextPopup(win);
}

function openTabContextPopup(win, tab) {
  var evt = new Event("");
  tab.dispatchEvent(evt);
  win.document.getElementById("tabContextMenu").openPopup(
    tab, "end_after", 0, 0, true, false, evt);
}

function closeTabContextPopup(win) {
  win.document.getElementById("tabContextMenu").hidePopup();
}

