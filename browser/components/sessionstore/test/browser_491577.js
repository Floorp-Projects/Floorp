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
  /** Test for Bug 491577 **/
  
  // test setup
  waitForExplicitFinish();
  
  const REMEMBER = Date.now(), FORGET = Math.random();
  let test_state = {
    windows: [ { tabs: [{ entries: [{ url: "http://example.com/" }] }], selected: 1 } ],
    _closedWindows : [
      // _closedWindows[0]
      {
        tabs: [
          { entries: [{ url: "http://example.com/", title: "title" }] },
          { entries: [{ url: "http://mozilla.org/", title: "title" }] }
        ],
        selected: 2,
        title: FORGET,
        _closedTabs: []
      },
      // _closedWindows[1]
      {
        tabs: [
         { entries: [{ url: "http://mozilla.org/", title: "title" }] },
         { entries: [{ url: "http://example.com/", title: "title" }] },
         { entries: [{ url: "http://mozilla.org/", title: "title" }] },
        ],
        selected: 3,
        title: REMEMBER,
        _closedTabs: []
      },
      // _closedWindows[2]
      {
        tabs: [
          { entries: [{ url: "http://example.com/", title: "title" }] }
        ],
        selected: 1,
        title: FORGET,
        _closedTabs: [
          {
            state: {
              entries: [
                { url: "http://mozilla.org/", title: "title" },
                { url: "http://mozilla.org/again", title: "title" }
              ]
            },
            pos: 1,
            title: "title"
          },
          {
            state: {
              entries: [
                { url: "http://example.com", title: "title" }
              ]
            },
            title: "title"
          }
        ]
      }
    ]
  };
  let remember_count = 1;
  
  function countByTitle(aClosedWindowList, aTitle)
    aClosedWindowList.filter(function(aData) aData.title == aTitle).length;
  
  function testForError(aFunction) {
    try {
      aFunction();
      return false;
    }
    catch (ex) {
      return ex.name == "NS_ERROR_ILLEGAL_VALUE";
    }
  }
  
  // open a window and add the above closed window list
  let newWin = openDialog(location, "_blank", "chrome,all,dialog=no");
  newWin.addEventListener("load", function(aEvent) {
    this.removeEventListener("load", arguments.callee, false);
    gPrefService.setIntPref("browser.sessionstore.max_windows_undo",
                            test_state._closedWindows.length);
    ss.setWindowState(newWin, JSON.stringify(test_state), true);
    
    let closedWindows = JSON.parse(ss.getClosedWindowData());
    is(closedWindows.length, test_state._closedWindows.length,
       "Closed window list has the expected length");
    is(countByTitle(closedWindows, FORGET),
       test_state._closedWindows.length - remember_count,
       "The correct amount of windows are to be forgotten");
    is(countByTitle(closedWindows, REMEMBER), remember_count,
       "Everything is set up.");
    
    // all of the following calls with illegal arguments should throw NS_ERROR_ILLEGAL_VALUE
    ok(testForError(function() ss.forgetClosedWindow(-1)),
       "Invalid window for forgetClosedWindow throws");
    ok(testForError(function() ss.forgetClosedWindow(test_state._closedWindows.length + 1)),
       "Invalid window for forgetClosedWindow throws");
	   
    // Remove third window, then first window
    ss.forgetClosedWindow(2);
    ss.forgetClosedWindow(null);
    
    closedWindows = JSON.parse(ss.getClosedWindowData());
    is(closedWindows.length, remember_count,
       "The correct amount of windows were removed");
    is(countByTitle(closedWindows, FORGET), 0,
       "All windows specifically forgotten were indeed removed");
    is(countByTitle(closedWindows, REMEMBER), remember_count,
       "... and windows not specifically forgetten weren't.");

    // clean up
    newWin.close();
    gPrefService.clearUserPref("browser.sessionstore.max_windows_undo");
    finish();
  }, false);
}
