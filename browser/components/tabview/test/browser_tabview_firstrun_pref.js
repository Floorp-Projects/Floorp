/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let prefsBranch = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefService).
                  getBranch("browser.panorama.");
let originalPrefState;

function test() {
  waitForExplicitFinish();

  ok(!TabView.isVisible(), "Main window TabView is hidden");

  originalPrefState = experienced();

  prefsBranch.setBoolPref("experienced_first_run", false);
  ok(!experienced(), "set to not experienced");

  newWindowWithTabView(checkFirstRun, function() {
    // open tabview doesn't count as first use experience so setting it manually
    prefsBranch.setBoolPref("experienced_first_run", true);
    ok(experienced(), "we're now experienced");

    newWindowWithTabView(checkNotFirstRun, endGame);
  });
}

function experienced() {
  return prefsBranch.prefHasUserValue("experienced_first_run") &&
    prefsBranch.getBoolPref("experienced_first_run");
}

function checkFirstRun(win) {
  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  
  // Welcome tab disabled by bug 626754. To be fixed via bug 626926.
  is(win.gBrowser.tabs.length, 1, "There should be one tab");
  
  let groupItems = contentWindow.GroupItems.groupItems;
  is(groupItems.length, 1, "There should be one group");
  is(groupItems[0].getChildren().length, 1, "...with one child");

  ok(!experienced(), "we're not experienced");
}

function checkNotFirstRun(win) {
  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  
  is(win.gBrowser.tabs.length, 1, "There should be one tab");
  
  let groupItems = contentWindow.GroupItems.groupItems;
  is(groupItems.length, 1, "There should be one group");
  is(groupItems[0].getChildren().length, 1, "...with one child");
}

function endGame() {
  ok(!TabView.isVisible(), "Main window TabView is still hidden");
  ok(experienced(), "should finish as experienced");

  prefsBranch.setBoolPref("experienced_first_run", originalPrefState);

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
