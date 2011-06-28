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

function provideWindow(aCallback, aURL, aFeatures) {
  function callback() {
    executeSoon(function () {
      aCallback(win);
    });
  }

  let win = openDialog(getBrowserURL(), "", aFeatures || "chrome,all,dialog=no", aURL);

  whenWindowLoaded(win, function () {
    if (!aURL) {
      callback();
      return;
    }
    win.gBrowser.selectedBrowser.addEventListener("load", function() {
      win.gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
      callback();
    }, true);
  });
}

function whenWindowLoaded(aWin, aCallback) {
  aWin.addEventListener("load", function () {
    aWin.removeEventListener("load", arguments.callee, false);
    executeSoon(function () {
      aCallback(aWin);
    });
  }, false);
}

function test() {
  // This test takes quite some time, and timeouts frequently, so we require
  // more time to run.
  // See Bug 518970.
  requestLongerTimeout(2);

  waitForExplicitFinish();  

  // helper function that does the actual testing
  function openWindowRec(windowsToOpen, expectedResults, recCallback) {
    // do actual checking
    if (!windowsToOpen.length) {
      let closedWindowData = JSON.parse(ss.getClosedWindowData());
      let numPopups = closedWindowData.filter(function(el, i, arr) {
        return el.isPopup;
      }).length;
      let numNormal = ss.getClosedWindowCount() - numPopups;
      // #ifdef doesn't work in browser-chrome tests, so do a simple regex on platform
      let oResults = navigator.platform.match(/Mac/) ? expectedResults.mac
                                                     : expectedResults.other;
      is(numPopups, oResults.popup,
         "There were " + oResults.popup + " popup windows to repoen");
      is(numNormal, oResults.normal,
         "There were " + oResults.normal + " normal windows to repoen");

      // cleanup & return
      executeSoon(recCallback);
      return;
    }
    // hack to force window to be considered a popup (toolbar=no didn't work)
    let winData = windowsToOpen.shift();
    let settings = "chrome,dialog=no," +
                   (winData.isPopup ? "all=no" : "all");
    let url = "http://example.com/?window=" + windowsToOpen.length;

    provideWindow(function (win) {
      win.close();
      openWindowRec(windowsToOpen, expectedResults, recCallback);
    }, url, settings);
  }

  let windowsToOpen = [{isPopup: false},
                       {isPopup: false},
                       {isPopup: true},
                       {isPopup: true},
                       {isPopup: true}];
  let expectedResults = {mac: {popup: 3, normal: 0},
                         other: {popup: 3, normal: 1}};
  let windowsToOpen2 = [{isPopup: false},
                        {isPopup: false},
                        {isPopup: false},
                        {isPopup: false},
                        {isPopup: false}];
  let expectedResults2 = {mac: {popup: 0, normal: 3},
                          other: {popup: 0, normal: 3}};
  openWindowRec(windowsToOpen, expectedResults, function() {
    openWindowRec(windowsToOpen2, expectedResults2, finish);
  });
}
