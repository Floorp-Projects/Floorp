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
 * The Original Code is a test for bug 622835.
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

  newWindowWithTabView(onTabViewShown);
}

function onTabViewShown(win) {
  let contentWindow = win.TabView.getContentWindow();

  let finishTest = function () {
    win.addEventListener('tabviewhidden', function () {
      win.removeEventListener('tabviewhidden', arguments.callee, false);
      win.close();
      finish();
    }, false);
    win.TabView.hide();
  }

  // do not let the group arrange itself
  contentWindow.GroupItems.pauseArrange();

  // let's create a groupItem small enough to get stacked
  let groupItem = new contentWindow.GroupItem([], {
    immediately: true,
    bounds: {left: 20, top: 20, width: 100, height: 100}
  });

  contentWindow.GroupItems.setActiveGroupItem(groupItem);

  // we need seven tabs at least to reproduce this
  for (var i=0; i<7; i++)
    win.gBrowser.loadOneTab('about:blank', {inBackground: true});

  // finally let group arrange
  contentWindow.GroupItems.resumeArrange();

  let tabItem6 = groupItem.getChildren()[5];
  let tabItem7 = groupItem.getChildren()[6];  
  ok(!tabItem6.getHidden(), 'the 6th child must not be hidden');
  ok(tabItem7.getHidden(), 'the 7th child must be hidden');

  finishTest();
}
