/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug-603750-websocket.html";
const TEST_URI2 = "data:text/html;charset=utf-8,Web Console test for " +
                  "bug 603750: Web Socket errors";

add_task(function* () {
  yield loadTab(TEST_URI2);

  let hud = yield openConsole();

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI);

  yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        text: "ws://0.0.0.0:81",
        source: { url: "test-bug-603750-websocket.js" },
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
      },
      {
        text: "ws://0.0.0.0:82",
        source: { url: "test-bug-603750-websocket.js" },
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
      },
    ]
  });
});
