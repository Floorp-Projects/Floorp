/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;
  let groupItemId;
  let prefix = 'start';

  let assertTabViewIsHidden = function () {
    ok(!TabView.isVisible(), prefix + ': tabview is hidden');
  }

  let assertNumberOfGroups = function (num) {
    is(cw.GroupItems.groupItems.length, num, prefix + ': there should be ' + num + ' groups');
  }

  let assertNumberOfTabs = function (num) {
    is(gBrowser.visibleTabs.length, num, prefix + ': there should be ' + num + ' tabs');
  }

  let assertNumberOfPinnedTabs = function (num) {
    is(gBrowser._numPinnedTabs, num, prefix + ': there should be ' + num + ' pinned tabs');
  }

  let assertGroupItemPreserved = function () {
    is(cw.GroupItems.groupItems[0].id, groupItemId, prefix + ': groupItem was preserved');
  }

  let assertValidPrerequisites = function () {
    assertNumberOfTabs(1);
    assertNumberOfGroups(1);
    assertNumberOfPinnedTabs(0);
    assertTabViewIsHidden();
  }

  let createTab = function (url) {
    return gBrowser.loadOneTab(url || 'http://mochi.test:8888/', {inBackground: true});
  }

  let createBlankTab = function () {
    return createTab('about:blank');
  }

  let finishTest = function () {
    prefix = 'finish';
    assertValidPrerequisites();
    finish();
  }

  let testUndoCloseWithSelectedBlankTab = function () {
    prefix = 'unpinned';
    let tab = createTab();
    assertNumberOfTabs(2);

    afterAllTabsLoaded(function () {
      gBrowser.removeTab(tab);
      assertNumberOfTabs(1);
      assertNumberOfPinnedTabs(0);

      restoreTab(function () {
        prefix = 'unpinned-restored';
        assertValidPrerequisites();
        assertGroupItemPreserved();

        createBlankTab();
        afterAllTabsLoaded(testUndoCloseWithSelectedBlankPinnedTab);
      }, 0);
    });
  }

  let testUndoCloseWithSelectedBlankPinnedTab = function () {
    prefix = 'pinned';
    assertNumberOfTabs(2);

    afterAllTabsLoaded(function () {
      gBrowser.removeTab(gBrowser.tabs[0]);
      gBrowser.pinTab(gBrowser.tabs[0]);

      registerCleanupFunction(function () {
        let tab = gBrowser.tabs[0];
        if (tab.pinned)
          gBrowser.unpinTab(tab);
      });

      assertNumberOfTabs(1);
      assertNumberOfPinnedTabs(1);

      restoreTab(function () {
        prefix = 'pinned-restored';
        assertValidPrerequisites();
        assertGroupItemPreserved();

        createBlankTab();
        gBrowser.removeTab(gBrowser.tabs[0]);

        afterAllTabsLoaded(finishTest);
      }, 0);
    });
  }

  waitForExplicitFinish();
  registerCleanupFunction(function () TabView.hide());

  showTabView(function () {
    hideTabView(function () {
      cw = TabView.getContentWindow();
      groupItemId = cw.GroupItems.groupItems[0].id;

      assertValidPrerequisites();
      testUndoCloseWithSelectedBlankTab();
    });
  });
}
