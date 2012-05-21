/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 514751 (Wallpaper) **/

  waitForExplicitFinish();

  let state = {
    windows: [{
      tabs: [{
        entries: [
          { url: "http://www.mozilla.org/projects/minefield/", title: "Minefield Start Page" },
          {}
        ]
      }]
    }]
  };

  var theWin = openDialog(location, "", "chrome,all,dialog=no");
  theWin.addEventListener("load", function () {
    theWin.removeEventListener("load", arguments.callee, false);

    executeSoon(function () {
      var gotError = false;
      try {
        ss.setWindowState(theWin, JSON.stringify(state), true);
      } catch (e) {
        if (/NS_ERROR_MALFORMED_URI/.test(e))
          gotError = true;
      }
      ok(!gotError, "Didn't get a malformed URI error.");
      theWin.close();
      finish();
    });
  }, false);
}

