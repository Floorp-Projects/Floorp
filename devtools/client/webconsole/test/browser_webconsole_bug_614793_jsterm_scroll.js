/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
                 "bug 614793: jsterm result scroll";

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  yield testScrollPosition(hud);
});

function* testScrollPosition(hud) {
  hud.jsterm.clearOutput();

  let scrollNode = hud.ui.outputWrapper;

  for (let i = 0; i < 150; i++) {
    yield ContentTask.spawn(gBrowser.selectedBrowser, i, function* (i) {
      content.console.log("test message " + i);
    });
  }

  let oldScrollTop = -1;

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test message 149",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  oldScrollTop = scrollNode.scrollTop;
  isnot(oldScrollTop, 0, "scroll location is not at the top");

  let msg = yield hud.jsterm.execute("'hello world'");

  isnot(scrollNode.scrollTop, oldScrollTop, "scroll location updated");

  oldScrollTop = scrollNode.scrollTop;

  msg.scrollIntoView(false);

  is(scrollNode.scrollTop, oldScrollTop, "scroll location is the same");
}
