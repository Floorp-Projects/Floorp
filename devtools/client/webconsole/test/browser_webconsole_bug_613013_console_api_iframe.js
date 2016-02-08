/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug-613013-console-api-iframe.html";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  BrowserReload();

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foobarBug613013",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });
});
