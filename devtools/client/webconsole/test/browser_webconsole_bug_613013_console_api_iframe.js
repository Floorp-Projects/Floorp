/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
