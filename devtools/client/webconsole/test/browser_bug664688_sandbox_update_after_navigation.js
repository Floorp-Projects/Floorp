/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests if the JSTerm sandbox is updated when the user navigates from one
// domain to another, in order to avoid permission denied errors with a sandbox
// created for a different origin.

"use strict";

var test = asyncTest(function* () {
  const TEST_URI1 = "http://example.com/browser/devtools/client/webconsole/" +
                    "test/test-console.html";
  const TEST_URI2 = "http://example.org/browser/devtools/client/webconsole/" +
                    "test/test-console.html";

  yield loadTab(TEST_URI1);
  let hud = yield openConsole();

  hud.jsterm.clearOutput();
  hud.jsterm.execute("window.location.href");

  info("wait for window.location.href");

  let msgForLocation1 = {
    webconsole: hud,
    messages: [
      {
        name: "window.location.href jsterm input",
        text: "window.location.href",
        category: CATEGORY_INPUT,
      },
      {
        name: "window.location.href result is displayed",
        text: TEST_URI1,
        category: CATEGORY_OUTPUT,
      },
    ],
  };

  yield waitForMessages(msgForLocation1);

  // load second url
  content.location = TEST_URI2;
  yield loadBrowser(gBrowser.selectedBrowser);

  is(hud.outputNode.textContent.indexOf("Permission denied"), -1,
     "no permission denied errors");

  hud.jsterm.clearOutput();
  hud.jsterm.execute("window.location.href");

  info("wait for window.location.href after page navigation");

  yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "window.location.href jsterm input",
        text: "window.location.href",
        category: CATEGORY_INPUT,
      },
      {
        name: "window.location.href result is displayed",
        text: TEST_URI2,
        category: CATEGORY_OUTPUT,
      },
    ],
  });

  is(hud.outputNode.textContent.indexOf("Permission denied"), -1,
     "no permission denied errors");

  gBrowser.goBack();

  yield waitForSuccess({
    name: "go back",
    validator: function() {
      return content.location.href == TEST_URI1;
    },
  });

  hud.jsterm.clearOutput();
  executeSoon(() => {
    hud.jsterm.execute("window.location.href");
  });

  info("wait for window.location.href after goBack()");
  yield waitForMessages(msgForLocation1);
  is(hud.outputNode.textContent.indexOf("Permission denied"), -1,
     "no permission denied errors");
});
