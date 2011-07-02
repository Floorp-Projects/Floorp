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
 * Simon Bünzli <zeniko@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Kraft <morac99-firefox@yahoo.com>
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
  /** Test for Bug 461634 **/

  waitForExplicitFinish();

  const REMEMBER = Date.now(), FORGET = Math.random();
  let test_state = { windows: [{ "tabs": [{ "entries": [] }], _closedTabs: [
    { state: { entries: [{ url: "http://www.example.net/" }] }, title: FORGET },
    { state: { entries: [{ url: "http://www.example.net/" }] }, title: REMEMBER },
    { state: { entries: [{ url: "http://www.example.net/" }] }, title: FORGET },
    { state: { entries: [{ url: "http://www.example.net/" }] }, title: REMEMBER },
  ] }] };
  let remember_count = 2;

  function countByTitle(aClosedTabList, aTitle)
    aClosedTabList.filter(function(aData) aData.title == aTitle).length;

  function testForError(aFunction) {
    try {
      aFunction();
      return false;
    }
    catch (ex) {
      return ex.name == "NS_ERROR_ILLEGAL_VALUE";
    }
  }

  // open a window and add the above closed tab list
  let newWin = openDialog(location, "", "chrome,all,dialog=no");
  newWin.addEventListener("load", function(aEvent) {
    newWin.removeEventListener("load", arguments.callee, false);

    gPrefService.setIntPref("browser.sessionstore.max_tabs_undo",
                            test_state.windows[0]._closedTabs.length);
    ss.setWindowState(newWin, JSON.stringify(test_state), true);

    let closedTabs = JSON.parse(ss.getClosedTabData(newWin));
    is(closedTabs.length, test_state.windows[0]._closedTabs.length,
       "Closed tab list has the expected length");
    is(countByTitle(closedTabs, FORGET),
       test_state.windows[0]._closedTabs.length - remember_count,
       "The correct amout of tabs are to be forgotten");
    is(countByTitle(closedTabs, REMEMBER), remember_count,
       "Everything is set up.");

    // all of the following calls with illegal arguments should throw NS_ERROR_ILLEGAL_VALUE
    ok(testForError(function() ss.forgetClosedTab({}, 0)),
       "Invalid window for forgetClosedTab throws");
    ok(testForError(function() ss.forgetClosedTab(newWin, -1)),
       "Invalid tab for forgetClosedTab throws");
    ok(testForError(function() ss.forgetClosedTab(newWin, test_state.windows[0]._closedTabs.length + 1)),
       "Invalid tab for forgetClosedTab throws");

    // Remove third tab, then first tab
    ss.forgetClosedTab(newWin, 2);
    ss.forgetClosedTab(newWin, null);

    closedTabs = JSON.parse(ss.getClosedTabData(newWin));
    is(closedTabs.length, remember_count,
       "The correct amout of tabs was removed");
    is(countByTitle(closedTabs, FORGET), 0,
       "All tabs specifically forgotten were indeed removed");
    is(countByTitle(closedTabs, REMEMBER), remember_count,
       "... and tabs not specifically forgetten weren't.");

    // clean up
    newWin.close();
    gPrefService.clearUserPref("browser.sessionstore.max_tabs_undo");
    finish();
  }, false);
}
