/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let newTab;
let win;

function test() {
  waitForExplicitFinish();

  win = window.openDialog(getBrowserURL(), "_blank", "all,dialog=no", "about:blank");

  let onLoad = function() {
    win.removeEventListener("load", onLoad, false);

    newTab = win.gBrowser.addTab();

    let onTabViewFrameInitialized = function() {
      win.removeEventListener(
        "tabviewframeinitialized", onTabViewFrameInitialized, false);

      win.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
      win.TabView.toggle();
    }
    win.addEventListener(
      "tabviewframeinitialized", onTabViewFrameInitialized, false);
    win.TabView.updateContextMenu(
      newTab, win.document.getElementById("context_tabViewMenuPopup"));
  }
  win.addEventListener("load", onLoad, false);
}

function onTabViewWindowLoaded() {
  win.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(win.TabView.isVisible(), "Tab View is visible");

  let contentWindow = win.document.getElementById("tab-view").contentWindow;

  is(contentWindow.GroupItems.groupItems.length, 1, "Has only one group");

  let groupItem = contentWindow.GroupItems.groupItems[0];
  let tabItems = groupItem.getChildren();

  is(tabItems[tabItems.length - 1].tab, newTab, "The new tab exists in the group");

  win.gBrowser.removeTab(newTab);
  whenWindowObservesOnce(win, "domwindowclosed", function() {
    finish();
  });
  win.close();
}

function whenWindowObservesOnce(win, topic, func) {
    let windowWatcher = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
      .getService(Components.interfaces.nsIWindowWatcher);
    let origWin = win;
    let origTopic = topic;
    let origFunc = func;        
    function windowObserver(aSubject, aTopic, aData) {
      let theWin = aSubject.QueryInterface(Ci.nsIDOMWindow);
      if (origWin && theWin != origWin)
        return;
      if(aTopic == origTopic) {
          windowWatcher.unregisterNotification(windowObserver);
          origFunc.apply(this, []);
      }
    }
    windowWatcher.registerNotification(windowObserver);
}
