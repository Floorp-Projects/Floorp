/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that errors still show up in the Web Console after a page reload.
// See bug 580030: the error handler fails silently after page reload.
// https://bugzilla.mozilla.org/show_bug.cgi?id=580030

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-error.html";

function test() {
  Task.spawn(function*() {
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
