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
 * The Original Code is a test for bug 616729.
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

  let createGroupItem = function () {
    let bounds = new cw.Rect(20, 20, 400, 200);
    let groupItem = new cw.GroupItem([], {bounds: bounds, immediately: true});
    cw.GroupItems.setActiveGroupItem(groupItem);

    for (let i=0; i<5; i++)
      gBrowser.loadOneTab('about:blank', {inBackground: true});

    return groupItem;
  }

  let assertCorrectItemOrder = function (items) {
    for (let i=1; i<items.length; i++) {
      if (items[i-1].tab._tPos > items[i].tab._tPos) {
        ok(false, 'tabs were correctly reordered');
        break;
      }
    }
  }

  let testVariousTabOrders = function () {
    let groupItem = createGroupItem();
    let [tab1, tab2, tab3, tab4, tab5] = groupItem.getChildren();

    // prepare tests
    let tests = [];
    tests.push([tab1, tab2, tab3, tab4, tab5]);
    tests.push([tab5, tab4, tab3, tab2, tab1]);
    tests.push([tab1, tab2, tab3, tab4]);
    tests.push([tab4, tab3, tab2, tab1]);
    tests.push([tab1, tab2, tab3]);
    tests.push([tab1, tab2, tab3]);
    tests.push([tab1, tab3, tab2]);
    tests.push([tab2, tab1, tab3]);
    tests.push([tab2, tab3, tab1]);
    tests.push([tab3, tab1, tab2]);
    tests.push([tab3, tab2, tab1]);
    tests.push([tab1, tab2]);
    tests.push([tab2, tab1]);
    tests.push([tab1]);

    // test reordering of empty groups - removes the last tab and causes
    // the groupItem to close
    tests.push([]);

    while (tests.length) {
      let test = tests.shift();

      // prepare
      let items = groupItem.getChildren();
      while (test.length < items.length)
        items[items.length-1].close();

      let orig = cw.Utils.copy(items);
      items.sort(function (a, b) test.indexOf(a) - test.indexOf(b));

      // order and check
      groupItem.reorderTabsBasedOnTabItemOrder();
      assertCorrectItemOrder(items);

      // revert to original item order
      items.sort(function (a, b) orig.indexOf(a) - orig.indexOf(b));
      groupItem.reorderTabsBasedOnTabItemOrder();
    }

    testMoveBetweenGroups();
  }

  let testMoveBetweenGroups = function () {
    let groupItem = createGroupItem();
    let groupItem2 = createGroupItem();
    
    afterAllTabsLoaded(function () {
      // move item from group1 to group2
      let child = groupItem.getChild(2);
      groupItem.remove(child);

      groupItem2.add(child, {index: 3});
      groupItem2.reorderTabsBasedOnTabItemOrder();

      assertCorrectItemOrder(groupItem.getChildren());
      assertCorrectItemOrder(groupItem2.getChildren());

      // move item from group2 to group1
      child = groupItem2.getChild(1);
      groupItem2.remove(child);

      groupItem.add(child, {index: 1});
      groupItem.reorderTabsBasedOnTabItemOrder();

      assertCorrectItemOrder(groupItem.getChildren());
      assertCorrectItemOrder(groupItem2.getChildren());

      // cleanup
      groupItem.addSubscriber(groupItem, 'groupHidden', function () {
        groupItem.removeSubscriber(groupItem, 'groupHidden');
        groupItem.closeHidden();
        groupItem2.closeAll();
      });

      groupItem2.addSubscriber(groupItem, 'groupHidden', function () {
        groupItem2.removeSubscriber(groupItem, 'groupHidden');
        groupItem2.closeHidden();
        hideTabView(finish);
      });

      groupItem.closeAll();
    });
  }

  waitForExplicitFinish();

  showTabView(function () {
    cw = TabView.getContentWindow();
    afterAllTabsLoaded(testVariousTabOrders);
  });
}
