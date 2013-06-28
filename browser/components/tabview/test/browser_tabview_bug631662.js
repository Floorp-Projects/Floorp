/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;
  let prefix;
  let timestamp;

  let storeTimestamp = function () {
    timestamp = cw.TabItems._lastUpdateTime;
  }

  let checkTimestamp = function () {
    is(timestamp, cw.TabItems._lastUpdateTime, prefix +
       ": tabs were not updated");
  }

  let actionAddTab = function () {
    storeTimestamp();
    gBrowser.addTab("about:mozilla");

    afterAllTabsLoaded(function () {
      checkTimestamp();
      next();
    });
  }

  let actionMoveTab = function () {
    storeTimestamp();
    gBrowser.moveTabTo(gBrowser.tabs[0], 1);
    gBrowser.moveTabTo(gBrowser.tabs[1], 0);
    checkTimestamp();
    next();
  }

  let actionSelectTab = function () {
    storeTimestamp();
    gBrowser.selectedTab = gBrowser.tabs[1]
    gBrowser.selectedTab = gBrowser.tabs[0]
    checkTimestamp();
    next();
  }

  let actionRemoveTab = function () {
    storeTimestamp();
    gBrowser.removeTab(gBrowser.tabs[1]);
    checkTimestamp();
    next();
  }

  let actions = [
    {name: "add", func: actionAddTab},
    {name: "move", func: actionMoveTab},
    {name: "select", func: actionSelectTab},
    {name: "remove", func: actionRemoveTab}
  ];

  let next = function () {
    let action = actions.shift();

    if (action) {
      prefix = action.name;
      action.func();
    } else {
      finish();
    }
  }

  waitForExplicitFinish();

  showTabView(function () {
    cw = TabView.getContentWindow();
    hideTabView(next);
  });
}
