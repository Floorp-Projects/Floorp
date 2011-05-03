/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

let stateStartup = {windows:[
  {tabs:[{entries:[{url:"about:home"}]}], extData:{"tabview-last-session-group-name":"title"}}
]};

function test() {
  let assertWindowTitle = function (win, title) {
    let browser = win.gBrowser.tabs[0].linkedBrowser;
    let winTitle = win.gBrowser.getWindowTitleForBrowser(browser);
    is(winTitle.indexOf(title), 0, "title starts with '" + title + "'");
  };

  let testGroupNameChange = function (win) {
    showTabView(function () {
      let cw = win.TabView.getContentWindow();
      let groupItem = cw.GroupItems.groupItems[0];
      groupItem.setTitle("new-title");

      hideTabView(function () {
        assertWindowTitle(win, "new-title");
        waitForFocus(finish);
      }, win);
    }, win);
  };

  waitForExplicitFinish();

  newWindowWithState(stateStartup, function (win) {
    registerCleanupFunction(function () win.close());
    assertWindowTitle(win, "title");
    testGroupNameChange(win);
  });
}

function newWindowWithState(state, callback) {
  let opts = "chrome,all,dialog=no,height=800,width=800";
  let win = window.openDialog(getBrowserURL(), "_blank", opts);

  whenWindowLoaded(win, function () {
    ss.setWindowState(win, JSON.stringify(state), true);
  });

  whenWindowStateReady(win, function () {
    win.close();
    win = ss.undoCloseWindow(0);
    whenWindowLoaded(win, function () callback(win));
  });
}

function whenWindowLoaded(win, callback) {
  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);
    executeSoon(callback);
  }, false);
}

function whenWindowStateReady(win, callback) {
  win.addEventListener("SSWindowStateReady", function onReady() {
    win.removeEventListener("SSWindowStateReady", onReady, false);
    executeSoon(callback);
  }, false);
}
