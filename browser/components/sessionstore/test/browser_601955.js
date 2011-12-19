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
 * The Original Code is sessionstore test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Mehdi Mulani <mmmulani@uwaterloo.ca>
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
 * ***** END LICENSE BLOCK *****/

// This tests that pinning/unpinning a tab, on its own, eventually triggers a
// session store.

function test() {
  waitForExplicitFinish();
  // We speed up the interval between session saves to ensure that the test
  // runs quickly.
  Services.prefs.setIntPref("browser.sessionstore.interval", 2000);

  // Loading a tab causes a save state and this is meant to catch that event.
  waitForSaveState(testBug601955_1);

  // Assumption: Only one window is open and it has one tab open.
  gBrowser.addTab("about:mozilla");
}

function testBug601955_1() {
  // Because pinned tabs are at the front of |gBrowser.tabs|, pinning tabs
  // re-arranges the |tabs| array.
  ok(!gBrowser.tabs[0].pinned, "first tab should not be pinned yet");
  ok(!gBrowser.tabs[1].pinned, "second tab should not be pinned yet");

  waitForSaveState(testBug601955_2);
  gBrowser.pinTab(gBrowser.tabs[0]);
}

function testBug601955_2() {
  let state = JSON.parse(ss.getBrowserState());
  ok(state.windows[0].tabs[0].pinned, "first tab should be pinned by now");
  ok(!state.windows[0].tabs[1].pinned, "second tab should still not be pinned");

  waitForSaveState(testBug601955_3);
  gBrowser.unpinTab(window.gBrowser.tabs[0]);
}

function testBug601955_3() {
  let state = JSON.parse(ss.getBrowserState());
  ok(!state.windows[0].tabs[0].pinned, "first tab should not be pinned");
  ok(!state.windows[0].tabs[1].pinned, "second tab should not be pinned");

  done();
}

function done() {
  gBrowser.removeTab(window.gBrowser.tabs[0]);

  Services.prefs.clearUserPref("browser.sessionstore.interval");

  executeSoon(finish);
}
