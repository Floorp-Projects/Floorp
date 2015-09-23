/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let createGroupItem = function (aWindow) {
    let cw = aWindow.TabView.getContentWindow();
    let bounds = new cw.Rect(20, 20, 400, 200);
    let groupItem = new cw.GroupItem([], {bounds: bounds, immediately: true});
    cw.UI.setActive(groupItem);

    let groupItemId = groupItem.id;
    registerCleanupFunction(function() {
      let groupItem = cw.GroupItems.groupItem(groupItemId);
      if (groupItem)
        groupItem.close();
    });

    for (let i=0; i<3; i++)
      aWindow.gBrowser.addTab('about:blank');
  }

  let assertTabViewIsHidden = function (aWindow, prefix) {
    ok(!aWindow.TabView.isVisible(), prefix + ': tabview is hidden');
  }

  let assertNumberOfTabs = function (aWindow, prefix, num) {
    is(aWindow.gBrowser.tabs.length, num, prefix + ': there are ' + num + ' tabs');
  }

  let assertNumberOfPinnedTabs = function (aWindow, prefix, num) {
    is(aWindow.gBrowser._numPinnedTabs, num, prefix + ': there are ' + num + ' pinned tabs');
  }

  let assertNumberOfGroups = function (aCW, prefix, num) {
    is(aCW.GroupItems.groupItems.length, num, prefix + ': there are ' + num + ' groups');
  }

  let assertOneTabInGroup = function (prefix, groupItem) {
    is(groupItem.getChildren().length, 1, prefix + ': group contains one tab');
  }

  let assertValidPrerequisites = function (aWindow, prefix) {
    assertNumberOfTabs(aWindow, prefix, 1);
    assertNumberOfPinnedTabs(aWindow, prefix, 0);
    assertTabViewIsHidden(aWindow, prefix);
  }

  let assertValidSetup = function (aWindow, prefix) {
    let cw = aWindow.TabView.getContentWindow();
    assertNumberOfGroups(cw, prefix, 2);
    assertNumberOfTabs(aWindow, prefix, 4);
    assertNumberOfPinnedTabs(aWindow, prefix, 2);

    let [group1, group2] = cw.GroupItems.groupItems;
    assertOneTabInGroup(prefix, group1);
    assertOneTabInGroup(prefix, group2);
  }

  let testStateAfterEnteringPB = function (aWindow, aCallback) {
    let prefix = 'window is private';
    ok(PrivateBrowsingUtils.isWindowPrivate(aWindow), prefix);

    assertTabViewIsHidden(aWindow, prefix);

    showTabView(function () {
      let cw = aWindow.TabView.getContentWindow();

      assertNumberOfGroups(cw, prefix, 1);
      assertNumberOfTabs(aWindow, prefix, 1);
      assertOneTabInGroup(prefix, cw.GroupItems.groupItems[0]);
      aCallback();
    }, aWindow);
  }

  let testStateAfterLeavingPB = function (aWindow) {
    let prefix = 'window is not private';
    ok(!PrivateBrowsingUtils.isWindowPrivate(aWindow), prefix);

    assertTabViewIsHidden(aWindow, prefix);

    showTabView(function () {
      assertValidSetup(aWindow, prefix);
      finishTest(aWindow);
    }, aWindow);
  }

  let finishTest = function (aWindow) {
    let cw = aWindow.TabView.getContentWindow();

    // Remove pinned tabs
    aWindow.gBrowser.removeTab(aWindow.gBrowser.tabs[0]);
    aWindow.gBrowser.removeTab(aWindow.gBrowser.tabs[0]);

    cw.GroupItems.groupItems[1].closeAll();

    hideTabView(function () {
      assertValidPrerequisites(aWindow, 'exit');
      assertNumberOfGroups(cw, 'exit', 1);
      aWindow.close();
      finish();
    }, aWindow);
  }

  let testOnWindow = function(aIsPrivate, aCallback) {
    let win = OpenBrowserWindow({private: aIsPrivate});
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      executeSoon(function() { aCallback(win) });
    }, false);
  }

  waitForExplicitFinish();
  testOnWindow(false, function(publicWindow) {
    registerCleanupFunction(() => publicWindow.TabView.hide());
    assertValidPrerequisites(publicWindow, 'start');

    showTabView(function () {
      let cw = publicWindow.TabView.getContentWindow();
      assertNumberOfGroups(cw, 'start', 1);
      createGroupItem(publicWindow);

      afterAllTabsLoaded(function () {
        // Setup
        let groupItems = cw.GroupItems.groupItems;
        let [tabItem1, tabItem2, ] = groupItems[1].getChildren();
        publicWindow.gBrowser.pinTab(tabItem1.tab);
        publicWindow.gBrowser.pinTab(tabItem2.tab);

        assertValidSetup(publicWindow, 'setup');
        hideTabView(function() {
          testOnWindow(true, function(privateWindow) {
            testStateAfterEnteringPB(privateWindow, function() {
              privateWindow.close();
              hideTabView(function() {
                testStateAfterLeavingPB(publicWindow);
              }, publicWindow);
            });
          });
        }, publicWindow);
      });
    }, publicWindow);
  });
}
