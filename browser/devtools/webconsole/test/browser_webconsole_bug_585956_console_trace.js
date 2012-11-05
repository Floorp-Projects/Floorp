/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-585956-console-trace.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded() {
  browser.removeEventListener("load", tabLoaded, true);

  openConsole(null, function(hud) {
    content.location.reload();

    waitForSuccess({
      name: "stacktrace message",
      validatorFn: function()
      {
        return hud.outputNode.querySelector(".hud-log");
      },
      successFn: performChecks,
      failureFn: finishTest,
    });
  });
}

function performChecks() {
  // The expected stack trace object.
  let stacktrace = [
    { filename: TEST_URI, lineNumber: 9, functionName: "window.foobar585956c", language: 2 },
    { filename: TEST_URI, lineNumber: 14, functionName: "foobar585956b", language: 2 },
    { filename: TEST_URI, lineNumber: 18, functionName: "foobar585956a", language: 2 },
    { filename: TEST_URI, lineNumber: 21, functionName: null, language: 2 }
  ];

  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];

  let node = HUD.outputNode.querySelector(".hud-log");
  ok(node, "found trace log node");
  ok(node._stacktrace, "found stacktrace object");
  is(node._stacktrace.toSource(), stacktrace.toSource(), "stacktrace is correct");
  isnot(node.textContent.indexOf("bug-585956"), -1, "found file name");

  finishTest();
}
