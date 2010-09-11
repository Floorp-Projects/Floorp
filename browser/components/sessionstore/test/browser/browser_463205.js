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
  /** Test for Bug 463205 **/
  
  waitForExplicitFinish();
  
  let rootDir = getRootDirectory(gTestPath);
  let testURL = rootDir + "browser_463205_sample.html";

  let doneURL = "done";

  let mainURL = testURL;
  let frame1URL = "data:text/html,<input%20id='original'>";
  let frame2URL = rootDir + "browser_463205_helper.html";
  let frame3URL = "data:text/html,mark2";

  let frameCount = 0;
  
  let tab = gBrowser.addTab(testURL);
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    // wait for all frames to load completely
    if (frame1URL != doneURL && aEvent.target.location.href == frame1URL) {
      frame1URL = doneURL;
      if (frameCount++ < 3) {
        return;
      }
    }
    if (frame2URL != doneURL && aEvent.target.location.href == frame2URL) {
      frame2URL = doneURL;
      if (frameCount++ < 3) {
        return;
      }
    }
    if (frame3URL != doneURL && aEvent.target.location.href == frame3URL) {
      frame3URL = doneURL;
      if (frameCount++ < 3) {
        return;
      }
    }
    if (mainURL != doneURL && aEvent.target.location.href == mainURL) {
      mainURL = doneURL;
      if (frameCount++ < 3) {
        return;
      }
    }
    if (frameCount < 3) {
      return;
    }
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    
    function typeText(aTextField, aValue) {
      aTextField.value = aValue;
      
      let event = aTextField.ownerDocument.createEvent("UIEvents");
      event.initUIEvent("input", true, true, aTextField.ownerDocument.defaultView, 0);
      aTextField.dispatchEvent(event);
    }
    
    let uniqueValue = "Unique: " + Math.random();
    let win = tab.linkedBrowser.contentWindow;
    typeText(win.frames[0].document.getElementById("original"), uniqueValue);
    typeText(win.frames[1].document.getElementById("original"), uniqueValue);

    mainURL = testURL;
    frame1URL = "http://mochi.test:8888/browser/" +
      "browser/components/sessionstore/test/browser/browser_463205_helper.html";
    frame2URL = rootDir + "browser_463205_helper.html";
    frame3URL = "data:text/html,mark2";

    frameCount = 0;

    let tab2 = gBrowser.duplicateTab(tab);
    tab2.linkedBrowser.addEventListener("load", function(aEvent) {
      // wait for all frames to load (and reload!) completely
      if (frame1URL != doneURL && aEvent.target.location.href == frame1URL) {
        frame1URL = doneURL;
        if (frameCount++ < 3) {
          return;
        }
      }
      if (frame2URL != doneURL && (aEvent.target.location.href == frame2URL ||
          aEvent.target.location.href == frame2URL + "#original")) {
        frame2URL = doneURL;
        if (frameCount++ < 3) {
          return;
        }
      }
      if (frame3URL != doneURL && aEvent.target.location.href == frame3URL) {
        frame3URL = doneURL;
        if (frameCount++ < 3) {
          return;
        }
      }
      if (mainURL != doneURL && aEvent.target.location.href == mainURL) {
        mainURL = doneURL;
        if (frameCount++ < 3) {
          return;
        }
      }
      if (frameCount < 3) {
        return;
      }
      tab2.linkedBrowser.removeEventListener("load", arguments.callee, true);

      let win = tab2.linkedBrowser.contentWindow;
      isnot(win.frames[0].document.getElementById("original").value, uniqueValue,
            "subframes must match URL to get text restored");
      is(win.frames[0].document.getElementById("original").value, "preserve me",
         "subframes must match URL to get text restored");
      is(win.frames[1].document.getElementById("original").value, uniqueValue,
         "text still gets restored for all other subframes");
      
      // clean up
      gBrowser.removeTab(tab2);
      gBrowser.removeTab(tab);
      
      finish();
    }, true);
  }, true);
}
