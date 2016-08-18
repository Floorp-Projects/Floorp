/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/mochitest/test-console.html";

add_task(function* () {
  let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");
  let ui = toolbox.getCurrentPanel().hud.ui;

  ok(ui.jsterm, "jsterm exists");
  ok(ui.newConsoleOutput, "newConsoleOutput exists");

  // @TODO: fix proptype errors and actually emit 'new-messages' event
  // let receivedLog = ui.once("new-messages");
  // yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
  //   content.wrappedJSObject.doLogs();
  // });
  // yield receivedLog;
});
