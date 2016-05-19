/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure non-toplevel security errors are displayed

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console subresource STS " +
                 "warning test";
const TEST_DOC = "https://example.com/browser/devtools/client/webconsole/" +
                 "test/test_bug1092055_shouldwarn.html";
const SAMPLE_MSG = "specified a header that could not be parsed successfully.";

add_task(function* () {
  let { browser } = yield loadTab(TEST_URI);

  let hud = yield openConsole();

  hud.jsterm.clearOutput();

  let loaded = loadBrowser(browser);
  content.location = TEST_DOC;
  yield loaded;

  yield waitForSuccess({
    name: "Subresource STS warning displayed successfully",
    validator: function () {
      return hud.outputNode.textContent.indexOf(SAMPLE_MSG) > -1;
    }
  });
});
