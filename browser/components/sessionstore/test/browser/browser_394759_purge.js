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
 * Paul O’Shannessy <paul@oshannessy.com>
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
  let pb = Cc["@mozilla.org/privatebrowsing;1"].getService(Ci.nsIPrivateBrowsingService);

  // utility functions
  function countClosedTabsByTitle(aClosedTabList, aTitle)
    aClosedTabList.filter(function (aData) aData.title == aTitle).length;

  function countOpenTabsByTitle(aOpenTabList, aTitle)
    aOpenTabList.filter(function (aData) aData.entries.some(function (aEntry) aEntry.title == aTitle)).length

  // backup old state
  let oldState = ss.getBrowserState();
  let oldState_wins = JSON.parse(oldState).windows.length;
  if (oldState_wins != 1)
    ok(false, "oldState in test_purge has " + oldState_wins + " windows instead of 1");

  // create a new state for testing
  const REMEMBER = Date.now(), FORGET = Math.random();
  let testState = {
    windows: [ { tabs: [{ entries: [{ url: "http://example.com/" }] }], selected: 1 } ],
    _closedWindows : [
      // _closedWindows[0]
      {
        tabs: [
          { entries: [{ url: "http://example.com/", title: REMEMBER }] },
          { entries: [{ url: "http://mozilla.org/", title: FORGET }] }
        ],
        selected: 2,
        title: "mozilla.org",
        _closedTabs: []
      },
      // _closedWindows[1]
      {
        tabs: [
         { entries: [{ url: "http://mozilla.org/", title: FORGET }] },
         { entries: [{ url: "http://example.com/", title: REMEMBER }] },
         { entries: [{ url: "http://example.com/", title: REMEMBER }] },
         { entries: [{ url: "http://mozilla.org/", title: FORGET }] },
         { entries: [{ url: "http://example.com/", title: REMEMBER }] }
        ],
        selected: 5,
        _closedTabs: []
      },
      // _closedWindows[2]
      {
        tabs: [
          { entries: [{ url: "http://example.com/", title: REMEMBER }] }
        ],
        selected: 1,
        _closedTabs: [
          {
            state: {
              entries: [
                { url: "http://mozilla.org/", title: FORGET },
                { url: "http://mozilla.org/again", title: "doesn't matter" }
              ]
            },
            pos: 1,
            title: FORGET
          },
          {
            state: {
              entries: [
                { url: "http://example.com", title: REMEMBER }
              ]
            },
            title: REMEMBER
          }
        ]
      }
    ]
  };
  
  // set browser to test state
  ss.setBrowserState(JSON.stringify(testState));

  // purge domain & check that we purged correctly for closed windows
  pb.removeDataFromDomain("mozilla.org");

  let closedWindowData = JSON.parse(ss.getClosedWindowData());

  // First set of tests for _closedWindows[0] - tests basics
  let win = closedWindowData[0];
  is(win.tabs.length, 1, "1 tab was removed");
  is(countOpenTabsByTitle(win.tabs, FORGET), 0,
     "The correct tab was removed");
  is(countOpenTabsByTitle(win.tabs, REMEMBER), 1,
     "The correct tab was remembered");
  is(win.selected, 1, "Selected tab has changed");
  is(win.title, REMEMBER, "The window title was correctly updated");

  // Test more complicated case 
  win = closedWindowData[1];
  is(win.tabs.length, 3, "2 tabs were removed");
  is(countOpenTabsByTitle(win.tabs, FORGET), 0,
     "The correct tabs were removed");
  is(countOpenTabsByTitle(win.tabs, REMEMBER), 3,
     "The correct tabs were remembered");
  is(win.selected, 3, "Selected tab has changed");
  is(win.title, REMEMBER, "The window title was correctly updated");

  // Tests handling of _closedTabs
  win = closedWindowData[2];
  is(countClosedTabsByTitle(win._closedTabs, REMEMBER), 1,
     "The correct number of tabs were removed, and the correct ones");
  is(countClosedTabsByTitle(win._closedTabs, FORGET), 0,
     "All tabs to be forgotten were indeed removed");

  // restore pre-test state
  ss.setBrowserState(oldState);
}
