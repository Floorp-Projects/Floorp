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
 * The Original Code is a test for bug 626368.
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

  let createGroupItem = function () {
    let bounds = new cw.Rect(20, 20, 150, 150);
    let groupItem = new cw.GroupItem([], {bounds: bounds, immediately: true});

    cw.GroupItems.setActiveGroupItem(groupItem);
    gBrowser.loadOneTab('about:blank', {inBackground: true});

    return groupItem;
  }

  let synthesizeMiddleMouseDrag = function (tabContainer, width) {
    EventUtils.synthesizeMouseAtCenter(tabContainer,
        {type: 'mousedown', button: 1}, cw);
    let rect = tabContainer.getBoundingClientRect();
    EventUtils.synthesizeMouse(tabContainer, rect.width / 2 + width,
        rect.height / 2, {type: 'mousemove', button: 1}, cw);
    EventUtils.synthesizeMouse(tabContainer, rect.width / 2 + width,
        rect.height / 2, {type: 'mouseup', button: 1}, cw);
  }

  let testDragAndDropWithMiddleMouseButton = function () {
    let groupItem = createGroupItem();
    let tabItem = groupItem.getChild(0);
    let tabContainer = tabItem.container;
    let bounds = tabItem.getBounds();

    // try to drag and move the mouse out of the tab
    synthesizeMiddleMouseDrag(tabContainer, 200);
    is(groupItem.getChild(0), tabItem, 'tabItem was not closed');
    ok(bounds.equals(tabItem.getBounds()), 'bounds did not change');

    // try to drag and let the mouse stay within tab bounds
    synthesizeMiddleMouseDrag(tabContainer, 10);
    ok(!groupItem.getChild(0), 'tabItem was closed');

    hideTabView(finish);
  }

  waitForExplicitFinish();

  showTabView(function () {
    cw = TabView.getContentWindow();
    afterAllTabsLoaded(testDragAndDropWithMiddleMouseButton);
  });
}
