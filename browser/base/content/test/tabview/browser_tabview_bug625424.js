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
 * The Original Code is a test for bug 625424.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
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

  let getGroupItem = function (index) {
    return cw.GroupItems.groupItems[index];
  }

  let createOrphan = function (callback) {
    let tab = gBrowser.loadOneTab('about:blank', {inBackground: true});
    afterAllTabsLoaded(function () {
      let tabItem = tab._tabViewTabItem;
      tabItem.parent.remove(tabItem);
      callback(tabItem);
    });
  }

  let hideGroupItem = function (groupItem, callback) {
    groupItem.addSubscriber(groupItem, 'groupHidden', function () {
      groupItem.removeSubscriber(groupItem, 'groupHidden');
      callback();
    });
    groupItem.closeAll();
  }

  let showGroupItem = function (groupItem, callback) {
    groupItem.addSubscriber(groupItem, 'groupShown', function () {
      groupItem.removeSubscriber(groupItem, 'groupShown');
      callback();
    });
    groupItem._unhide();
  }

  let assertNumberOfTabsInGroupItem = function (groupItem, numTabs) {
    is(groupItem.getChildren().length, numTabs,
        'there are ' + numTabs + ' tabs in this groupItem');
  }

  let testDragOnHiddenGroup = function () {
    createOrphan(function (orphan) {
      let groupItem = getGroupItem(0);
      hideGroupItem(groupItem, function () {
        let drag = orphan.container;
        let drop = groupItem.$undoContainer[0];

        assertNumberOfTabsInGroupItem(groupItem, 1);

        EventUtils.synthesizeMouseAtCenter(drag, {type: 'mousedown'});
        EventUtils.synthesizeMouseAtCenter(drop, {type: 'mousemove'});
        EventUtils.synthesizeMouseAtCenter(drop, {type: 'mouseup'});

        assertNumberOfTabsInGroupItem(groupItem, 1);

        showGroupItem(groupItem, function () {
          drop = groupItem.container;
          
          EventUtils.synthesizeMouseAtCenter(drag, {type: 'mousedown'});
          EventUtils.synthesizeMouseAtCenter(drop, {type: 'mousemove'});
          EventUtils.synthesizeMouseAtCenter(drop, {type: 'mouseup'});

          assertNumberOfTabsInGroupItem(groupItem, 2);
          
          orphan.close();
          hideTabView(finish);
        });
      });
    });
  }

  waitForExplicitFinish();

  showTabView(function () {
    cw = TabView.getContentWindow();
    testDragOnHiddenGroup();
  });
}
