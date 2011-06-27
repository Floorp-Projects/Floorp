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
 * Portions created by the Initial Developer are Copyright (C) 2009
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
  /** Test for Bug 477657 **/

  // Test fails randomly on OS X (bug 482975)
  if ("nsILocalFileMac" in Ci)
    return;

  waitForExplicitFinish();
  
  let newWin = openDialog(location, "_blank", "chrome,all,dialog=no");
  newWin.addEventListener("load", function(aEvent) {
    newWin.removeEventListener("load", arguments.callee, false);

    let newState = { windows: [{
      tabs: [{ entries: [] }],
      _closedTabs: [{
        state: { entries: [{ url: "about:" }]},
        title: "About:"
      }],
      sizemode: "maximized"
    }] };

    let uniqueKey = "bug 477657";
    let uniqueValue = "unik" + Date.now();

    ss.setWindowValue(newWin, uniqueKey, uniqueValue);
    is(ss.getWindowValue(newWin, uniqueKey), uniqueValue,
       "window value was set before the window was overwritten");
    ss.setWindowState(newWin, JSON.stringify(newState), true);

    // use setTimeout(..., 0) to mirror sss_restoreWindowFeatures
    setTimeout(function() {
      is(ss.getWindowValue(newWin, uniqueKey), "",
         "window value was implicitly cleared");

      is(newWin.windowState, newWin.STATE_MAXIMIZED,
         "the window was maximized");

      is(JSON.parse(ss.getClosedTabData(newWin)).length, 1,
         "the closed tab was added before the window was overwritten");
      delete newState.windows[0]._closedTabs;
      delete newState.windows[0].sizemode;
      ss.setWindowState(newWin, JSON.stringify(newState), true);

      setTimeout(function() {
        is(JSON.parse(ss.getClosedTabData(newWin)).length, 0,
           "closed tabs were implicitly cleared");

        is(newWin.windowState, newWin.STATE_MAXIMIZED,
           "the window remains maximized");
        newState.windows[0].sizemode = "normal";
        ss.setWindowState(newWin, JSON.stringify(newState), true);

        setTimeout(function() {
          isnot(newWin.windowState, newWin.STATE_MAXIMIZED,
                "the window was explicitly unmaximized");

          newWin.close();
          finish();
        }, 0);
      }, 0);
    }, 0);
  }, false);
}
