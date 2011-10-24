/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let pb = Cc['@mozilla.org/privatebrowsing;1'].
         getService(Ci.nsIPrivateBrowsingService);

function test() {
  let cw;

  let createGroupItem = function () {
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
      gBrowser.addTab('about:blank');
  }

  let assertTabViewIsHidden = function (prefix) {
    ok(!TabView.isVisible(), prefix + ': tabview is hidden');
  }

  let assertNumberOfTabs = function (prefix, num) {
    is(gBrowser.tabs.length, num, prefix + ': there are ' + num + ' tabs');
  }

  let assertNumberOfPinnedTabs = function (prefix, num) {
    is(gBrowser._numPinnedTabs, num, prefix + ': there are ' + num + ' pinned tabs');
  }

  let assertNumberOfGroups = function (prefix, num) {
    is(cw.GroupItems.groupItems.length, num, prefix + ': there are ' + num + ' groups');
  }

  let assertOneTabInGroup = function (prefix, groupItem) {
    is(groupItem.getChildren().length, 1, prefix + ': group contains one tab');
  }

  let assertValidPrerequisites = function (prefix) {
    assertNumberOfTabs(prefix, 1);
    assertNumberOfPinnedTabs(prefix, 0);
    assertTabViewIsHidden(prefix);
  }

  let assertValidSetup = function (prefix) {
    assertNumberOfGroups(prefix, 2);
    assertNumberOfTabs(prefix, 4);
    assertNumberOfPinnedTabs(prefix, 2);

    let [group1, group2] = cw.GroupItems.groupItems;
    assertOneTabInGroup(prefix, group1);
    assertOneTabInGroup(prefix, group2);
  }

  let testStateAfterEnteringPB = function () {
    let prefix = 'enter';
    ok(!pb.privateBrowsingEnabled, prefix + ': private browsing is disabled');
    registerCleanupFunction(function () {
      pb.privateBrowsingEnabled = false;
    });

    togglePrivateBrowsing(function () {
      assertTabViewIsHidden(prefix);

      showTabView(function () {
        assertNumberOfGroups(prefix, 1);
        assertNumberOfTabs(prefix, 1);
        assertOneTabInGroup(prefix, cw.GroupItems.groupItems[0]);
        hideTabView(testStateAfterLeavingPB);
      });
    });
  }

  let testStateAfterLeavingPB = function () {
    let prefix = 'leave';
    ok(pb.privateBrowsingEnabled, prefix + ': private browsing is enabled');

    togglePrivateBrowsing(function () {
      assertTabViewIsHidden(prefix);

      showTabView(function () {
        assertValidSetup(prefix);
        finishTest();
      });
    });
  }

  let finishTest = function () {
    // remove pinned tabs
    gBrowser.removeTab(gBrowser.tabs[0]);
    gBrowser.removeTab(gBrowser.tabs[0]);

    cw.GroupItems.groupItems[1].closeAll();

    hideTabView(function () {
      assertValidPrerequisites('exit');
      assertNumberOfGroups('exit', 1);
      finish();
    });
  }

  waitForExplicitFinish();
  registerCleanupFunction(function () TabView.hide());
  assertValidPrerequisites('start');

  showTabView(function () {
    cw = TabView.getContentWindow();
    assertNumberOfGroups('start', 1);

    createGroupItem();

    afterAllTabsLoaded(function () {
      // setup
      let groupItems = cw.GroupItems.groupItems;
      let [tabItem1, tabItem2, ] = groupItems[1].getChildren();
      gBrowser.pinTab(tabItem1.tab);
      gBrowser.pinTab(tabItem2.tab);

      assertValidSetup('setup');
      hideTabView(testStateAfterEnteringPB);
    });
  });
}
