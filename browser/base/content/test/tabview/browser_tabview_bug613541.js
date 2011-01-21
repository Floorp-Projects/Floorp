/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is a test for bug 613541.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Tim Taubert <tim.taubert@gmx.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function test() {
  let cw;
  let currentTest;

  let getGroupItem = function (index) {
    return cw.GroupItems.groupItems[index];
  }

  let createGroupItem = function (numTabs) {
    let bounds = new cw.Rect(20, 20, 200, 200);
    let groupItem = new cw.GroupItem([], {bounds: bounds, immediately: true});
    cw.GroupItems.setActiveGroupItem(groupItem);

    for (let i=0; i<numTabs || 0; i++)
      gBrowser.loadOneTab('about:blank', {inBackground: true});

    return groupItem;
  }

  let hideGroupItem = function (groupItem, callback) {
    groupItem.addSubscriber(groupItem, 'groupHidden', function () {
      groupItem.removeSubscriber(groupItem, 'groupHidden');
      callback();
    });
    groupItem.closeAll();
  }

  let closeGroupItem = function (groupItem, callback) {
    afterAllTabsLoaded(function () {
      hideGroupItem(groupItem, function () {
        groupItem.closeHidden();
        callback();
      });
    });
  }

  let tests = [];

  let next = function () {
    let test = tests.shift();

    if (test) {
      // check that the previous test left things as expected
      if (currentTest) {
        currentTest += ' (post-check)';
        assertTabViewIsHidden();
        assertNumberOfGroupItems(1);
        assertNumberOfTabs(1);
      }

      currentTest = test.name;
      showTabView(test.func);
    } else
      hideTabView(finish);
  }

  let assertTabViewIsHidden = function () {
    ok(!TabView.isVisible(), currentTest + ': tabview is hidden');
  }

  let assertNumberOfGroupItems = function (num) {
    is(cw.GroupItems.groupItems.length, num, currentTest + ': number of groupItems is equal to ' + num);
  }

  let assertNumberOfTabs = function (num) {
    is(gBrowser.tabs.length, num, currentTest + ': number of tabs is equal to ' + num);
  }

  let assertGroupItemRemoved = function (groupItem) {
    is(cw.GroupItems.groupItems.indexOf(groupItem), -1, currentTest + ': groupItem was removed');
  }

  let assertGroupItemExists = function (groupItem) {
    isnot(cw.GroupItems.groupItems.indexOf(groupItem), -1, currentTest + ': groupItem still exists');
  }

  // setup: 1 non-empty group
  // action: close the group
  // expected: new group with blank tab is created and zoomed into
  let testSingleGroup1 = function () {
    let groupItem = getGroupItem(0);
    closeGroupItem(groupItem, function () {
      assertNumberOfGroupItems(1);
      assertGroupItemRemoved(groupItem);
      whenTabViewIsHidden(next);
    });
  }

  // setup: 1 non-empty group
  // action: hide the group, exit panorama
  // expected: new group with blank tab is created and zoomed into
  let testSingleGroup2 = function () {
    let groupItem = getGroupItem(0);
    hideGroupItem(groupItem, function () {
      hideTabView(function () {
        assertNumberOfGroupItems(1);
        assertGroupItemRemoved(groupItem);
        next();
      });
    });
  }

  // setup: 2 non-empty groups
  // action: close one group
  // expected: nothing should happen
  let testNonEmptyGroup1 = function () {
    let groupItem = getGroupItem(0);
    let newGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(2);

    closeGroupItem(groupItem, function () {
      assertNumberOfGroupItems(1);
      assertGroupItemExists(newGroupItem);
      hideTabView(next);
    });
  }

  // setup: 2 non-empty groups
  // action: hide one group, exit panorama
  // expected: nothing should happen
  let testNonEmptyGroup2 = function () {
    let groupItem = getGroupItem(0);
    let newGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(2);

    hideGroupItem(groupItem, function () {
      hideTabView(function () {
        assertNumberOfGroupItems(1);
        assertGroupItemRemoved(groupItem);
        assertGroupItemExists(newGroupItem);
        next();
      });
    });
  }

  // setup: 1 pinned tab, 1 empty group
  // action: exit panorama
  // expected: nothing should happen
  let testPinnedTab1 = function () {
    gBrowser.pinTab(gBrowser.selectedTab);

    let groupItem = getGroupItem(0);
    hideTabView(function () {
      assertNumberOfGroupItems(1);
      assertGroupItemExists(groupItem);
      gBrowser.unpinTab(gBrowser.selectedTab);
      next();
    });
  }

  // setup: 1 pinned tab
  // action: exit panorama
  // expected: new blank group is created
  let testPinnedTab2 = function () {
    gBrowser.pinTab(gBrowser.selectedTab);
    getGroupItem(0).close();

    hideTabView(function () {
      assertNumberOfTabs(1);
      assertNumberOfGroupItems(1);
      gBrowser.unpinTab(gBrowser.selectedTab);
      next();
    });
  }

  // setup: 1 pinned tab, 1 empty group, 1 non-empty group
  // action: close non-empty group
  // expected: nothing should happen
  let testPinnedTab3 = function () {
    gBrowser.pinTab(gBrowser.selectedTab);

    let groupItem = getGroupItem(0);
    let newGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(2);

    closeGroupItem(newGroupItem, function () {
      assertNumberOfGroupItems(1);
      assertGroupItemExists(groupItem);

      gBrowser.unpinTab(gBrowser.selectedTab);
      hideTabView(next);
    });
  }

  // setup: 1 pinned tab, 1 empty group, 1 non-empty group
  // action: hide non-empty group, exit panorama
  // expected: nothing should happen
  let testPinnedTab4 = function () {
    gBrowser.pinTab(gBrowser.selectedTab);

    let groupItem = getGroupItem(0);
    let newGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(2);

    hideGroupItem(newGroupItem, function () {
      hideTabView(function () {
        assertNumberOfGroupItems(1);
        assertGroupItemExists(groupItem);
        assertGroupItemRemoved(newGroupItem);
        gBrowser.unpinTab(gBrowser.selectedTab);
        next();
      });
    });
  }

  // setup: 1 orphan tab
  // action: exit panorama
  // expected: nothing should happen
  let testOrphanTab1 = function () {
    let groupItem = getGroupItem(0);
    let tabItem = groupItem.getChild(0);
    groupItem.remove(tabItem);

    hideTabView(function () {
      assertNumberOfGroupItems(0);
      createGroupItem().add(tabItem);
      next();
    });
  }

  // setup: 1 orphan tab, 1 non-empty group
  // action: close the group
  // expected: nothing should happen
  let testOrphanTab2 = function () {
    let groupItem = getGroupItem(0);
    let tabItem = groupItem.getChild(0);
    groupItem.remove(tabItem);

    assertNumberOfGroupItems(0);
    let newGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(1);

    closeGroupItem(newGroupItem, function () {
      assertNumberOfGroupItems(0);
      createGroupItem().add(tabItem);
      hideTabView(next);
    });
  }

  // setup: 1 orphan tab, 1 non-empty group
  // action: hide the group, exit panorama
  // expected: nothing should happen
  let testOrphanTab3 = function () {
    let groupItem = getGroupItem(0);
    let tabItem = groupItem.getChild(0);
    groupItem.remove(tabItem);

    assertNumberOfGroupItems(0);
    let newGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(1);

    hideGroupItem(newGroupItem, function () {
      hideTabView(function () {
        assertNumberOfGroupItems(0);
        createGroupItem().add(tabItem);
        next();
      });
    });
  }

  // setup: 1 non-empty group, 1 empty group
  // action: close non-empty group
  // expected: empty group is re-used, new tab is created and zoomed into
  let testEmptyGroup1 = function () {
    let groupItem = getGroupItem(0);
    let newGroupItem = createGroupItem(0);
    assertNumberOfGroupItems(2);

    closeGroupItem(groupItem, function () {
      assertNumberOfGroupItems(1);
      assertGroupItemExists(newGroupItem);
      whenTabViewIsHidden(next);
    });
  }

  // setup: 1 non-empty group, 1 empty group
  // action: hide non-empty group, exit panorama
  // expected: empty group is re-used, new tab is created and zoomed into
  let testEmptyGroup2 = function () {
    let groupItem = getGroupItem(0);
    let newGroupItem = createGroupItem(0);
    assertNumberOfGroupItems(2);

    hideGroupItem(groupItem, function () {
      hideTabView(function () {
        assertNumberOfGroupItems(1);
        assertGroupItemRemoved(groupItem);
        assertGroupItemExists(newGroupItem);
        next();
      });
    });
  }

  // setup: 1 hidden group, 1 non-empty group
  // action: close non-empty group
  // expected: nothing should happen
  let testHiddenGroup1 = function () {
    let groupItem = getGroupItem(0);
    let hiddenGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(2);

    hideGroupItem(hiddenGroupItem, function () {
      closeGroupItem(groupItem, function () {
        assertNumberOfGroupItems(1);
        assertGroupItemRemoved(groupItem);
        assertGroupItemExists(hiddenGroupItem);
        hideTabView(next);
      });
    });
  }

  // setup: 1 hidden group, 1 non-empty group
  // action: hide non-empty group, exit panorama
  // expected: new group with blank tab is created and zoomed into
  let testHiddenGroup2 = function () {
    let groupItem = getGroupItem(0);
    let hiddenGroupItem = createGroupItem(1);
    assertNumberOfGroupItems(2);

    hideGroupItem(hiddenGroupItem, function () {
      hideGroupItem(groupItem, function () {
        hideTabView(function () {
          assertNumberOfGroupItems(1);
          assertGroupItemRemoved(groupItem);
          assertGroupItemRemoved(hiddenGroupItem);
          next();
        });
      });
    });
  }

  tests.push({name: 'testSingleGroup1', func: testSingleGroup1});
  tests.push({name: 'testSingleGroup2', func: testSingleGroup2});

  tests.push({name: 'testNonEmptyGroup1', func: testNonEmptyGroup1});
  tests.push({name: 'testNonEmptyGroup2', func: testNonEmptyGroup2});

  tests.push({name: 'testPinnedTab1', func: testPinnedTab1});
  tests.push({name: 'testPinnedTab2', func: testPinnedTab2});
  tests.push({name: 'testPinnedTab3', func: testPinnedTab3});
  tests.push({name: 'testPinnedTab4', func: testPinnedTab4});

  tests.push({name: 'testOrphanTab1', func: testOrphanTab1});
  tests.push({name: 'testOrphanTab2', func: testOrphanTab2});
  tests.push({name: 'testOrphanTab3', func: testOrphanTab3});

  tests.push({name: 'testEmptyGroup1', func: testEmptyGroup1});
  tests.push({name: 'testEmptyGroup2', func: testEmptyGroup2});

  tests.push({name: 'testHiddenGroup1', func: testHiddenGroup1});
  tests.push({name: 'testHiddenGroup2', func: testHiddenGroup2}),

  waitForExplicitFinish();

  showTabView(function () {
    cw = TabView.getContentWindow();
    next();
  });
}
