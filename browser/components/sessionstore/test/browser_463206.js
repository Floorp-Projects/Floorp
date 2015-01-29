/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Test for Bug 463206 **/

  waitForExplicitFinish();

  let testURL = "http://mochi.test:8888/browser/" +
    "browser/components/sessionstore/test/browser_463206_sample.html";

  var frameCount = 0;
  let tab = gBrowser.addTab(testURL);
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    // wait for all frames to load completely
    if (frameCount++ < 5)
      return;
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    function typeText(aTextField, aValue) {
      aTextField.value = aValue;

      let event = aTextField.ownerDocument.createEvent("UIEvents");
      event.initUIEvent("input", true, true, aTextField.ownerDocument.defaultView, 0);
      aTextField.dispatchEvent(event);
    }

    let doc = tab.linkedBrowser.contentDocument;
    typeText(doc.getElementById("out1"), Date.now());
    typeText(doc.getElementsByName("1|#out2")[0], Math.random());
    typeText(doc.defaultView.frames[0].frames[1].document.getElementById("in1"), new Date());

    let tab2 = gBrowser.duplicateTab(tab);
    promiseTabRestored(tab2).then(() => {
      let doc = tab2.linkedBrowser.contentDocument;
      let win = tab2.linkedBrowser.contentWindow;
      isnot(doc.getElementById("out1").value,
            win.frames[1].document.getElementById("out1").value,
            "text isn't reused for frames");
      isnot(doc.getElementsByName("1|#out2")[0].value, "",
            "text containing | and # is correctly restored");
      is(win.frames[1].document.getElementById("out2").value, "",
            "id prefixes can't be faked");
      // Disabled for now, Bug 588077
      // isnot(win.frames[0].frames[1].document.getElementById("in1").value, "",
      //       "id prefixes aren't mixed up");
      is(win.frames[1].frames[0].document.getElementById("in1").value, "",
            "id prefixes aren't mixed up");

      // clean up
      gBrowser.removeTab(tab2);
      gBrowser.removeTab(tab);

      finish();
    });
  }, true);
}
