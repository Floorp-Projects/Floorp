/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "http://mochi.test:8888/browser/" +
                 "browser/components/sessionstore/test/browser_463206_sample.html";

add_task(async function() {
  // Add a new tab.
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_URL);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // "Type in" some random values.
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    function typeText(aTextField, aValue) {
      aTextField.value = aValue;

      let event = aTextField.ownerDocument.createEvent("UIEvents");
      event.initUIEvent("input", true, true, aTextField.ownerGlobal, 0);
      aTextField.dispatchEvent(event);
    }

    typeText(content.document.getElementById("out1"), Date.now());
    typeText(content.document.getElementsByName("1|#out2")[0], Math.random());
    typeText(content.frames[0].frames[1].document.getElementById("in1"), new Date());
  });

  // Duplicate the tab.
  let tab2 = gBrowser.duplicateTab(tab);
  await promiseTabRestored(tab2);

  // Query a few values from the top and its child frames.
  await ContentTask.spawn(tab2.linkedBrowser, null, async function() {
    Assert.notEqual(content.document.getElementById("out1").value,
      content.frames[1].document.getElementById("out1").value,
      "text isn't reused for frames");
    Assert.notEqual(content.document.getElementsByName("1|#out2")[0].value,
      "", "text containing | and # is correctly restored");
    Assert.equal(content.frames[1].document.getElementById("out2").value,
      "", "id prefixes can't be faked");
    // Disabled for now, Bug 588077
    // Assert.equal(content.frames[0].frames[1].document.getElementById("in1").value,
    //  "", "id prefixes aren't mixed up");
    Assert.equal(content.frames[1].frames[0].document.getElementById("in1").value,
      "", "id prefixes aren't mixed up");
  });

  // Cleanup.
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab);
});
