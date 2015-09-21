/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that warnings about ineffective iframe sandboxing are logged to the
// web console when necessary (and not otherwise).

"use strict";

const TEST_URI_WARNING = "http://example.com/browser/devtools/client/" +
                         "webconsole/test/test-bug-752559-ineffective-iframe-sandbox-warning0.html";
const TEST_URI_NOWARNING = [
  "http://example.com/browser/devtools/client/webconsole/test/test-bug-752559-ineffective-iframe-sandbox-warning1.html",
  "http://example.com/browser/devtools/client/webconsole/test/test-bug-752559-ineffective-iframe-sandbox-warning2.html",
  "http://example.com/browser/devtools/client/webconsole/test/test-bug-752559-ineffective-iframe-sandbox-warning3.html",
  "http://example.com/browser/devtools/client/webconsole/test/test-bug-752559-ineffective-iframe-sandbox-warning4.html",
  "http://example.com/browser/devtools/client/webconsole/test/test-bug-752559-ineffective-iframe-sandbox-warning5.html"
];

const INEFFECTIVE_IFRAME_SANDBOXING_MSG = "An iframe which has both " +
  "allow-scripts and allow-same-origin for its sandbox attribute can remove " +
  "its sandboxing.";
const SENTINEL_MSG = "testing ineffective sandboxing message";

function test() {
  loadTab(TEST_URI_WARNING).then(() => {
    openConsole().then((hud) => {
      content.console.log(SENTINEL_MSG);
      waitForMessages({
        webconsole: hud,
        messages: [
          {
            name: "Ineffective iframe sandboxing warning displayed successfully",
            text: INEFFECTIVE_IFRAME_SANDBOXING_MSG,
            category: CATEGORY_SECURITY,
            severity: SEVERITY_WARNING
          },
          {
            text: SENTINEL_MSG,
            severity: SEVERITY_LOG
          }
        ]
      }).then(() => {
        let msgs = hud.outputNode.querySelectorAll(".message[category=security]");
        is(msgs.length, 1, "one security message");
        testNoWarning(0);
      });
    })
  });
}

function testNoWarning(id) {
  loadTab(TEST_URI_NOWARNING[id]).then(() => {
    openConsole().then((hud) => {
      content.console.log(SENTINEL_MSG);
      waitForMessages({
        webconsole: hud,
        messages: [
          {
            text: SENTINEL_MSG,
            severity: SEVERITY_LOG
          }
        ]
      }).then(() => {
        let msgs = hud.outputNode.querySelectorAll(".message[category=security]");
        is(msgs.length, 0, "no security messages (case " + id + ")");

        id += 1;
        if (id < TEST_URI_NOWARNING.length) {
          testNoWarning(id);
        } else {
          finishTest();
        }
      });
    });
  });
}
