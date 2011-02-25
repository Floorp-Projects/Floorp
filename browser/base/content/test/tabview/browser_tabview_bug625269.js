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
 * The Original Code is bug 625269 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
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

  newWindowWithTabView(onTabViewWindowLoaded);
}

function onTabViewWindowLoaded(win) {
  win.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(win.TabView.isVisible(), "Tab View is visible");

  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  let [originalTab] = win.gBrowser.visibleTabs;

  let currentGroup = contentWindow.GroupItems.getActiveGroupItem();

  currentGroup.setSize(600, 600, true);
  currentGroup.setUserSize();

  let down1 = function down1(resized) {
    checkResized(currentGroup, 50, 50, false, "A little bigger", up1, contentWindow, win);
  };
  
  let up1 = function up1(resized) {
    checkResized(currentGroup, -400, -400, true, "Much smaller", down2, contentWindow, win);    
  }

  let down2 = function down2(resized) {
    checkResized(currentGroup, 400, 400, undefined,
      "Bigger after much smaller: TODO (bug 625668): the group should be resized!",
      up2, contentWindow, win);
  };
  
  let up2 = function up2(resized) {
    checkResized(currentGroup, -400, -400, undefined,
      "Much smaller: TODO (bug 625668): the group should be resized!",
      down3, contentWindow, win);    
  }

  let down3 = function down3(resized) {
    // reset the usersize of the group, so this should clear the "cramped" feeling.
    currentGroup.setSize(100,100,true);
    currentGroup.setUserSize();
    checkResized(currentGroup, 400, 400, false,
      "After clearing the cramp",
      up3, contentWindow, win);
  };
  
  let up3 = function up3(resized) {
    win.close();
    finish();
  }

  // start by making it a little smaller.
  checkResized(currentGroup, -50, -50, false, "A little smaller", down1, contentWindow, win);
}

function simulateResizeBy(xDiff, yDiff, win) {
  win = win || window;

  win.resizeBy(xDiff, yDiff);
}

function checkResized(item, xDiff, yDiff, expectResized, note, callback, contentWindow, win) {
  let originalBounds = new contentWindow.Rect(item.getBounds());
  simulateResizeBy(xDiff, yDiff, win);

  let newBounds = item.getBounds();
  let resized = !newBounds.equals(originalBounds);
  if (expectResized !== undefined)
    is(resized, expectResized, note + ": The group should " + 
      (expectResized ? "" : "not ") + "be resized");
  callback(resized);
}
