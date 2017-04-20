/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that message source links for js errors and console API calls open in
// the jsdebugger when clicked.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/test" +
                 "/test-bug-766001-js-console-links.html";

// Force the new debugger UI, in case this gets uplifted with the old
// debugger still turned on
Services.prefs.setBoolPref("devtools.debugger.new-debugger-frontend", true);
registerCleanupFunction(function* () {
  Services.prefs.clearUserPref("devtools.debugger.new-debugger-frontend");
});

function test() {
  let hud;

  requestLongerTimeout(2);
  Task.spawn(runner).then(finishTest);

  function* runner() {
    // On e10s, the exception is triggered in child process
    // and is ignored by test harness
    if (!Services.appinfo.browserTabsRemoteAutostart) {
      expectUncaughtException();
    }

    let {tab} = yield loadTab(TEST_URI);
    hud = yield openConsole(tab);

    let [exceptionRule, consoleRule] = yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "document.bar",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
      },
        {
          text: "Blah Blah",
          category: CATEGORY_WEBDEV,
          severity: SEVERITY_LOG,
        }],
    });

    let exceptionMsg = [...exceptionRule.matched][0];
    let consoleMsg = [...consoleRule.matched][0];
    let nodes = [exceptionMsg.querySelector(".message-location > .frame-link"),
                 consoleMsg.querySelector(".message-location > .frame-link")];
    ok(nodes[0], ".location node for the exception message");
    ok(nodes[1], ".location node for the console message");

    for (let i = 0; i < nodes.length; i++) {
      yield checkClickOnNode(i, nodes[i]);
      yield gDevTools.showToolbox(hud.target, "webconsole");
    }

    // check again the first node.
    yield checkClickOnNode(0, nodes[0]);
  }

  function* checkClickOnNode(index, node) {
    info("checking click on node index " + index);

    let url = node.getAttribute("data-url");
    ok(url, "source url found for index " + index);

    let line = node.getAttribute("data-line");
    ok(line, "found source line for index " + index);

    executeSoon(() => {
      EventUtils.sendMouseEvent({ type: "click" }, node.querySelector(".frame-link-filename"));
    });

    yield hud.ui.once("source-in-debugger-opened");

    let toolbox = yield gDevTools.getToolbox(hud.target);
    let dbg = toolbox.getPanel("jsdebugger");
    is(dbg._selectors().getSelectedSource(dbg._getState()).get("url"),
       url,
       "expected source url");
  }
}
