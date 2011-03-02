/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var prefsBranch = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefService).
                  getBranch("browser.panorama.");

function test() {
  waitForExplicitFinish();

  ok(!TabView.isVisible(), "Main window TabView is hidden");

  ok(experienced(), "should start as experienced");

  prefsBranch.setBoolPref("experienced_first_run", false);
  ok(!experienced(), "set to not experienced");

  newWindowWithTabView(checkFirstRun, part2);
}

function experienced() {
  return prefsBranch.prefHasUserValue("experienced_first_run") &&
    prefsBranch.getBoolPref("experienced_first_run");
}

function checkFirstRun(win) {
  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  
  // Welcome tab disabled by bug 626754. To be fixed via bug 626926.
  todo_is(win.gBrowser.tabs.length, 2, "There should be two tabs");
  
  let groupItems = contentWindow.GroupItems.groupItems;
  is(groupItems.length, 1, "There should be one group");
  is(groupItems[0].getChildren().length, 1, "...with one child");

  let orphanTabCount = contentWindow.GroupItems.getOrphanedTabs().length;
  // Welcome tab disabled by bug 626754. To be fixed via bug 626926.
  todo_is(orphanTabCount, 1, "There should also be an orphaned tab");
  
  ok(experienced(), "we're now experienced");
}

function part2() {
  newWindowWithTabView(checkNotFirstRun, endGame);
}

function checkNotFirstRun(win) {
  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  
  is(win.gBrowser.tabs.length, 1, "There should be one tab");
  
  let groupItems = contentWindow.GroupItems.groupItems;
  is(groupItems.length, 1, "There should be one group");
  is(groupItems[0].getChildren().length, 1, "...with one child");

  let orphanTabCount = contentWindow.GroupItems.getOrphanedTabs().length;
  is(orphanTabCount, 0, "There should also be no orphaned tabs");
}

function endGame() {
  ok(!TabView.isVisible(), "Main window TabView is still hidden");
  ok(experienced(), "should finish as experienced");
  finish();
}

function newWindowWithTabView(callback, completeCallback) {
  let charsetArg = "charset=" + window.content.document.characterSet;
  let win = window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no,height=800,width=800",
                              "about:blank", charsetArg, null, null, true);
  let onLoad = function() {
    win.removeEventListener("load", onLoad, false);
    let onShown = function() {
      win.removeEventListener("tabviewshown", onShown, false);
      callback(win);
      win.close();
      if (typeof completeCallback == "function")
        completeCallback();
    };
    win.addEventListener("tabviewshown", onShown, false);
    win.TabView.toggle();
  }
  win.addEventListener("load", onLoad, false);
}
