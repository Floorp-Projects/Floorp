/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", onLoad, false);
}

function onLoad() {
  browser.removeEventListener("DOMContentLoaded", onLoad, false);
  openConsole(null, function(hud) {
    content.console.log("a log message");

    waitForSuccess({
      name: "console.log message shown with an ID attribute",
      validatorFn: function()
      {
        let node = hud.outputNode.querySelector(".hud-msg-node");
        return node && node.getAttribute("id");
      },
      successFn: finishTest,
      failureFn: finishTest,
    });
  });
}
