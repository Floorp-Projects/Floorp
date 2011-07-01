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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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
  /** Test for Bug 524745 **/

  let uniqKey = "bug524745";
  let uniqVal = Date.now();

  waitForExplicitFinish();

  let window_B = openDialog(location, "_blank", "chrome,all,dialog=no");
  window_B.addEventListener("load", function(aEvent) {
    window_B.removeEventListener("load", arguments.callee, false);

      waitForFocus(function() {
        // Add identifying information to window_B
        ss.setWindowValue(window_B, uniqKey, uniqVal);
        let state = JSON.parse(ss.getBrowserState());
        let selectedWindow = state.windows[state.selectedWindow - 1];
        is(selectedWindow.extData && selectedWindow.extData[uniqKey], uniqVal,
           "selectedWindow is window_B");

        // Now minimize window_B. The selected window shouldn't have the secret data
        window_B.minimize();
        waitForFocus(function() {
          state = JSON.parse(ss.getBrowserState());
          selectedWindow = state.windows[state.selectedWindow - 1];
          ok(!selectedWindow.extData || !selectedWindow.extData[uniqKey],
             "selectedWindow is not window_B after minimizing it");

          // Now minimize the last open window (assumes no other tests left windows open)
          window.minimize();
          state = JSON.parse(ss.getBrowserState());
          is(state.selectedWindow, 0,
             "selectedWindow should be 0 when all windows are minimized");

          // Cleanup
          window.restore();
          window_B.close();
          finish();
        });
      }, window_B);
  }, false);
}
