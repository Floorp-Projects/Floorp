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
 * The Original Code is tabview bug 624953 test.
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
  waitForExplicitFinish();

  let finishTest = function (groupItem) {
    groupItem.addSubscriber(groupItem, 'groupHidden', function () {
      groupItem.removeSubscriber(groupItem, 'groupHidden');
      groupItem.closeHidden();
      hideTabView(finish);
    });

    groupItem.closeAll();
  }

  showTabView(function () {
    let cw = TabView.getContentWindow();

    let bounds = new cw.Rect(20, 20, 150, 200);
    let groupItem = new cw.GroupItem([], {bounds: bounds, immediately: true});
    cw.GroupItems.setActiveGroupItem(groupItem);

    for (let i=0; i<4; i++)
      gBrowser.loadOneTab('about:blank', {inBackground: true});

    ok(!groupItem._isStacked, 'groupItem is not stacked');
    cw.GroupItems.pauseArrange();

    groupItem.setSize(150, 150);
    groupItem.setUserSize();
    ok(!groupItem._isStacked, 'groupItem is still not stacked');

    cw.GroupItems.resumeArrange();
    ok(groupItem._isStacked, 'groupItem is now stacked');

    finishTest(groupItem);
  });
}
