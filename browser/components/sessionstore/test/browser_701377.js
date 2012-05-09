/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let state = {windows:[{tabs:[
  {entries:[{url:"http://example.com#1"}]},
  {entries:[{url:"http://example.com#2"}], hidden: true}
]}]};

function test() {
  waitForExplicitFinish();

  newWindowWithState(state, function (aWindow) {
    let tab = aWindow.gBrowser.tabs[1];
    ok(tab.hidden, "the second tab is hidden");

    let tabShown = false;
    let tabShowCallback = function () tabShown = true;
    tab.addEventListener("TabShow", tabShowCallback, false);

    let tabState = ss.getTabState(tab);
    ss.setTabState(tab, tabState);

    tab.removeEventListener("TabShow", tabShowCallback, false);
    ok(tab.hidden && !tabShown, "tab remains hidden");

    finish();
  });
}

// ----------
function newWindowWithState(aState, aCallback) {
  let opts = "chrome,all,dialog=no,height=800,width=800";
  let win = window.openDialog(getBrowserURL(), "_blank", opts);

  registerCleanupFunction(function () win.close());

  whenWindowLoaded(win, function onWindowLoaded(aWin) {
    ss.setWindowState(aWin, JSON.stringify(aState), true);
    executeSoon(function () aCallback(aWin));
  });
}
