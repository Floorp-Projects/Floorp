/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Make sure that the Web Console output does not break after we try to call
// console.dir() for objects that are not inspectable.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,test for bug 773466";

var test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  hud.jsterm.clearOutput(true);

  hud.jsterm.execute("console.log('fooBug773466a')");
  hud.jsterm.execute("myObj = Object.create(null)");
  hud.jsterm.execute("console.dir(myObj)");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "fooBug773466a",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    },
    {
      name: "console.dir output",
      consoleDir: "[object Object]",
    }],
  });

  content.console.log("fooBug773466b");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "fooBug773466b",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });
});
