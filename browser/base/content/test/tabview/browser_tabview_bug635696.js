/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;

  let createGroupItem = function () {
    let groupItem = createGroupItemWithBlankTabs(window, 200, 200, 10, 1);

    let groupItemId = groupItem.id;
    registerCleanupFunction(function() {
      let groupItem = cw.GroupItems.groupItem(groupItemId);
      if (groupItem)
        groupItem.close();
    });

    return groupItem;
  }

  let createOrphan = function () {
    let tab = gBrowser.loadOneTab('about:blank', {inBackground: true});
    registerCleanupFunction(function () {
      if (gBrowser.tabs.length > 1)
        gBrowser.removeTab(gBrowser.tabs[1])
    });

    let tabItem = tab._tabViewTabItem;
    tabItem.parent.remove(tabItem);

    return tabItem;
  }

  let testSingleGroupItem = function () {
    let groupItem = cw.GroupItems.groupItems[0];
    is(cw.GroupItems.getActiveGroupItem(), groupItem, "groupItem is active");

    let tabItem = groupItem.getChild(0);
    is(cw.UI.getActiveTab(), tabItem, "tabItem is active");

    hideGroupItem(groupItem, function () {
      is(cw.GroupItems.getActiveGroupItem(), null, "groupItem is not active");
      unhideGroupItem(groupItem, function () {
        is(cw.GroupItems.getActiveGroupItem(), groupItem, "groupItem is active again");
        is(cw.UI.getActiveTab(), tabItem, "tabItem is still active");
        next();
      });
    });
  }

  let testTwoGroupItems = function () {
    let groupItem = cw.GroupItems.groupItems[0];
    let tabItem = groupItem.getChild(0);

    let groupItem2 = createGroupItem();
    let tabItem2 = groupItem2.getChild(0);

    hideGroupItem(groupItem, function () {
      is(cw.UI.getActiveTab(), tabItem2, "tabItem2 is active");
      unhideGroupItem(groupItem, function () {
        cw.UI.setActiveTab(tabItem);
        closeGroupItem(groupItem2, next);
      });
    });
  }

  let testOrphanTab = function () {
    let groupItem = cw.GroupItems.groupItems[0];
    let tabItem = groupItem.getChild(0);
    let tabItem2 = createOrphan();

    hideGroupItem(groupItem, function () {
      is(cw.UI.getActiveTab(), tabItem2, "tabItem2 is active");
      unhideGroupItem(groupItem, function () {
        cw.UI.setActiveTab(tabItem);
        tabItem2.close();
        next();
      });
    });
  }

  let tests = [testSingleGroupItem, testTwoGroupItems, testOrphanTab];

  let next = function () {
    let test = tests.shift();
    if (test)
      test();
    else
      hideTabView(finishTest);
  }

  let finishTest = function () {
    is(cw.GroupItems.groupItems.length, 1, "there is one groupItem");
    is(gBrowser.tabs.length, 1, "there is one tab");
    ok(!TabView.isVisible(), "tabview is hidden");

    finish();
  }

  waitForExplicitFinish();

  showTabView(function () {
    registerCleanupFunction(function () TabView.hide());
    cw = TabView.getContentWindow();
    next();
  });
}
