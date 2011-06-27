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
 * Ehsan Akhgari <ehsan.akhgari@gmail.com>.
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
  /** Test for Bug 495495 **/
  
  waitForExplicitFinish();

  let newWin = openDialog(location, "_blank", "chrome,all,dialog=no,toolbar=yes");
  newWin.addEventListener("load", function() {
    newWin.removeEventListener("load", arguments.callee, false);

    executeSoon(function() {
      let state1 = ss.getWindowState(newWin);
      newWin.close();

      newWin = openDialog(location, "_blank",
        "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar=no,location,personal,directories,dialog=no");
      newWin.addEventListener("load", function() {
        newWin.removeEventListener("load", arguments.callee, false);

        executeSoon(function() {
          let state2 = ss.getWindowState(newWin);
          newWin.close();

          function testState(state, expected, callback) {
            let win = openDialog(location, "_blank", "chrome,all,dialog=no");
            win.addEventListener("load", function() {
              win.removeEventListener("load", arguments.callee, false);

              is(win.gURLBar.readOnly, false,
                 "URL bar should not be read-only before setting the state");
              is(win.gURLBar.getAttribute("enablehistory"), "true",
                 "URL bar autocomplete should be enabled before setting the state");
              ss.setWindowState(win, state, true);
              is(win.gURLBar.readOnly, expected.readOnly,
                 "URL bar read-only state should be restored correctly");
              is(win.gURLBar.getAttribute("enablehistory"), expected.enablehistory,
                 "URL bar autocomplete state should be restored correctly");

              win.close();
              executeSoon(callback);
            }, false);
          }

          testState(state1, {readOnly: false, enablehistory: "true"}, function() {
            testState(state2, {readOnly: true, enablehistory: "false"}, finish);
          });
        });
      }, false);
    });
  }, false);
}
