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
  /** Test for Bug 465223 **/

  // test setup
  waitForExplicitFinish();

  let uniqueKey1 = "bug 465223.1";
  let uniqueKey2 = "bug 465223.2";
  let uniqueValue1 = "unik" + Date.now();
  let uniqueValue2 = "pi != " + Math.random();

  // open a window and set a value on it
  let newWin = openDialog(location, "_blank", "chrome,all,dialog=no");
  newWin.addEventListener("load", function(aEvent) {
    newWin.removeEventListener("load", arguments.callee, false);

    ss.setWindowValue(newWin, uniqueKey1, uniqueValue1);

    let newState = { windows: [{ tabs:[{ entries: [] }], extData: {} }] };
    newState.windows[0].extData[uniqueKey2] = uniqueValue2;
    ss.setWindowState(newWin, JSON.stringify(newState), false);

    is(newWin.gBrowser.tabs.length, 2,
       "original tab wasn't overwritten");
    is(ss.getWindowValue(newWin, uniqueKey1), uniqueValue1,
       "window value wasn't overwritten when the tabs weren't");
    is(ss.getWindowValue(newWin, uniqueKey2), uniqueValue2,
       "new window value was correctly added");

    newState.windows[0].extData[uniqueKey2] = uniqueValue1;
    ss.setWindowState(newWin, JSON.stringify(newState), true);

    is(newWin.gBrowser.tabs.length, 1,
       "original tabs were overwritten");
    is(ss.getWindowValue(newWin, uniqueKey1), "",
       "window value was cleared");
    is(ss.getWindowValue(newWin, uniqueKey2), uniqueValue1,
       "window value was correctly overwritten");

    // clean up
    newWin.close();
    finish();
  }, false);
}
