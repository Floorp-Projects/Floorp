/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 495495 **/

  waitForExplicitFinish();

  let newWin = openDialog(location, "_blank", "chrome,all,dialog=no,toolbar=yes");
  promiseWindowLoaded(newWin).then(() => {
    let state1 = ss.getWindowState(newWin);
    BrowserTestUtils.closeWindow(newWin).then(() => {

      newWin = openDialog(location, "_blank",
        "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar=no,location,personal,directories,dialog=no");
      promiseWindowLoaded(newWin).then(() => {
        let state2 = ss.getWindowState(newWin);

        function testState(state, expected, callback) {
          let win = openDialog(location, "_blank", "chrome,all,dialog=no");
          promiseWindowLoaded(win).then(() => {

            is(win.gURLBar.readOnly, false,
               "URL bar should not be read-only before setting the state");
            ss.setWindowState(win, state, true);
            is(win.gURLBar.readOnly, expected.readOnly,
               "URL bar read-only state should be restored correctly");

            BrowserTestUtils.closeWindow(win).then(callback);
          });
        }

        BrowserTestUtils.closeWindow(newWin).then(() => {
          testState(state1, {readOnly: false}, function() {
            testState(state2, {readOnly: true}, finish);
          });
        });
      });
    });
  });
}
