/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that console.dirxml works as intended.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,Web Console test for bug 922212:
  Add console.dirxml`;

var test = asyncTest(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  hud.jsterm.clearOutput();

  // Should work like console.log(window)
  hud.jsterm.execute("console.dirxml(window)");

  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "console.dirxml(window) output:",
      text: /Window \u2192/,
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  hud.jsterm.clearOutput();

  hud.jsterm.execute("console.dirxml(document.body)");

  // Should work like console.log(document.body);
  [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "console.dirxml(document.body) output:",
      text: "<body>",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });
  let msg = [...result.matched][0];
  yield checkLinkToInspector(true, msg);
});

