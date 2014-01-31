/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 597315 - Frameset history does not work properly when restoring a tab **/
  waitForExplicitFinish();

  let testURL = getRootDirectory(gTestPath) + "browser_597315_index.html";
  let tab = gBrowser.addTab(testURL);
  gBrowser.selectedTab = tab;

  waitForLoadsInBrowser(tab.linkedBrowser, 4, function() {
    let browser_b = tab.linkedBrowser.contentDocument.getElementsByTagName("frame")[1];
    let document_b = browser_b.contentDocument;
    let links = document_b.getElementsByTagName("a");

    // We're going to click on the first link, so listen for another load event
    waitForLoadsInBrowser(tab.linkedBrowser, 1, function() {
      waitForLoadsInBrowser(tab.linkedBrowser, 1, function() {

        gBrowser.removeTab(tab);
        // wait for 4 loads again...
        let newTab = ss.undoCloseTab(window, 0);

        waitForLoadsInBrowser(newTab.linkedBrowser, 4, function() {
          gBrowser.goBack();
          waitForLoadsInBrowser(newTab.linkedBrowser, 1, function() {

            let expectedURLEnds = ["a.html", "b.html", "c1.html"];
            let frames = newTab.linkedBrowser.contentDocument.getElementsByTagName("frame");
            for (let i = 0; i < frames.length; i++) {
              is(frames[i].contentDocument.location,
                 getRootDirectory(gTestPath) + "browser_597315_" + expectedURLEnds[i],
                 "frame " + i + " has the right url");
            }
            gBrowser.removeTab(newTab);
            executeSoon(finish);
          });
        });
      });
      EventUtils.sendMouseEvent({type:"click"}, links[1], browser_b.contentWindow);
    });
    EventUtils.sendMouseEvent({type:"click"}, links[0], browser_b.contentWindow);
  });
}

// helper function
function waitForLoadsInBrowser(aBrowser, aLoadCount, aCallback) {
  let loadCount = 0;
  aBrowser.addEventListener("load", function(aEvent) {
    if (++loadCount < aLoadCount)
      return;

    aBrowser.removeEventListener("load", arguments.callee, true);
    aCallback();
  }, true);
}
