/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that errors still show up in the Web Console after a page reload.
// See bug 580030: the error handler fails silently after page reload.
// https://bugzilla.mozilla.org/show_bug.cgi?id=580030

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-error.html";

function test() {
  Task.spawn(function* () {
    const {tab} = yield loadTab(TEST_URI);
    const hud = yield openConsole(tab);
    info("console opened");

    executeSoon(() => {
      hud.jsterm.clearOutput();
      info("wait for reload");
      content.location.reload();
    });

    yield hud.target.once("navigate");
    info("target navigated");

    let button = content.document.querySelector("button");
    ok(button, "button found");

    // On e10s, the exception is triggered in child process
    // and is ignored by test harness
    if (!Services.appinfo.browserTabsRemoteAutostart) {
      expectUncaughtException();
    }

    EventUtils.sendMouseEvent({type: "click"}, button, content);

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "fooBazBaz is not defined",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
      }],
    });
  }).then(finishTest);
}
