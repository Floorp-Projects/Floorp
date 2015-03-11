/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "http://mochi.test:8888/browser/" +
                 "browser/components/sessionstore/test/browser_463206_sample.html";

add_task(function* () {
  // Add a new tab.
  let tab = gBrowser.addTab(TEST_URL);
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // "Type in" some random values.
  yield ContentTask.spawn(tab.linkedBrowser, null, function* () {
    function typeText(aTextField, aValue) {
      aTextField.value = aValue;

      let event = aTextField.ownerDocument.createEvent("UIEvents");
      event.initUIEvent("input", true, true, aTextField.ownerDocument.defaultView, 0);
      aTextField.dispatchEvent(event);
    }

    typeText(content.document.getElementById("out1"), Date.now());
    typeText(content.document.getElementsByName("1|#out2")[0], Math.random());
    typeText(content.frames[0].frames[1].document.getElementById("in1"), new Date());
  });

  // Duplicate the tab.
  let tab2 = gBrowser.duplicateTab(tab);
  yield promiseTabRestored(tab2);

  // Query a few values from the top and its child frames.
  let query = ContentTask.spawn(tab2.linkedBrowser, null, function* () {
    return [
      content.document.getElementById("out1").value,
      content.frames[1].document.getElementById("out1").value,
      content.document.getElementsByName("1|#out2")[0].value,
      content.frames[1].document.getElementById("out2").value,
      content.frames[0].frames[1].document.getElementById("in1").value,
      content.frames[1].frames[0].document.getElementById("in1").value
    ];
  });

  let [v1, v2, v3, v4, v5, v6] = yield query;
  isnot(v1, v2, "text isn't reused for frames");
  isnot(v3, "", "text containing | and # is correctly restored");
  is(v4, "", "id prefixes can't be faked");
  // Disabled for now, Bug 588077
  //is(v5, "", "id prefixes aren't mixed up");
  is(v6, "", "id prefixes aren't mixed up");

  // Cleanup.
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab);
});
