/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 463205 **/
  
  waitForExplicitFinish();
  
  let rootDir = "http://mochi.test:8888/browser/browser/components/sessionstore/test/";
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
      "browser/components/sessionstore/test/browser_463205_helper.html";
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
