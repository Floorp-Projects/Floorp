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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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
