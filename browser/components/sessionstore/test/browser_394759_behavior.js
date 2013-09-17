/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

    provideWindow(function onTestURLLoaded(win) {
      win.close();
      // Give it time to close
      executeSoon(function() {
        openWindowRec(windowsToOpen, expectedResults, recCallback);
      });
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
