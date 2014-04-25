/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;
  let win;
  let groupItemId;
  let prefix = 'start';

  let assertTabViewIsHidden = function () {
    ok(!win.TabView.isVisible(), prefix + ': tabview is hidden');
  }

  let assertNumberOfGroups = function (num) {
    is(cw.GroupItems.groupItems.length, num, prefix + ': there should be ' + num + ' groups');
  }

  let assertNumberOfTabs = function (num) {
    is(win.gBrowser.visibleTabs.length, num, prefix + ': there should be ' + num + ' tabs');
  }

  let assertNumberOfPinnedTabs = function (num) {
    is(win.gBrowser._numPinnedTabs, num, prefix + ': there should be ' + num + ' pinned tabs');
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
    return win.gBrowser.loadOneTab(url || 'http://mochi.test:8888/', {inBackground: true});
  }

  let createBlankTab = function () {
    return createTab('about:blank');
  }

  let finishTest = function () {
    prefix = 'finish';
    assertValidPrerequisites();
    promiseWindowClosed(win).then(finish);
  }

  let testUndoCloseWithSelectedBlankTab = function () {
    prefix = 'unpinned';
    let tab = createTab();
    assertNumberOfTabs(2);

    afterAllTabsLoaded(function () {
      win.gBrowser.removeTab(tab);
      assertNumberOfTabs(1);
      assertNumberOfPinnedTabs(0);

      restoreTab(function () {
        prefix = 'unpinned-restored';
        assertValidPrerequisites();
        assertGroupItemPreserved();

        createBlankTab();
        afterAllTabsLoaded(testUndoCloseWithSelectedBlankPinnedTab, win);
      }, 0, win);
    }, win);
  }

  let testUndoCloseWithSelectedBlankPinnedTab = function () {
    prefix = 'pinned';
    assertNumberOfTabs(2);

    afterAllTabsLoaded(function () {
      win.gBrowser.removeTab(win.gBrowser.tabs[0]);
      win.gBrowser.pinTab(win.gBrowser.tabs[0]);

      assertNumberOfTabs(1);
      assertNumberOfPinnedTabs(1);

      restoreTab(function () {
        prefix = 'pinned-restored';
        assertValidPrerequisites();
        assertGroupItemPreserved();

        createBlankTab();
        win.gBrowser.removeTab(win.gBrowser.tabs[0]);

        afterAllTabsLoaded(finishTest, win);
      }, 0, win);
    }, win);
  }

  waitForExplicitFinish();

  newWindowWithTabView(window => {
    win = window;

    hideTabView(function () {
      cw = win.TabView.getContentWindow();
      groupItemId = cw.GroupItems.groupItems[0].id;

      assertValidPrerequisites();
      testUndoCloseWithSelectedBlankTab();
    }, win);
  });
}
