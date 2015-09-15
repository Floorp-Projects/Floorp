/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that console.dir works as intended.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
                 "bug 659907: Expand console object with a dir method";

var test = asyncTest(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  hud.jsterm.clearOutput();

  hud.jsterm.execute("console.dir(document)");

  let varView = yield hud.jsterm.once("variablesview-fetched");

  yield findVariableViewProperties(varView, [
    {
      name: "__proto__.__proto__.querySelectorAll",
      value: "querySelectorAll()"
    },
    {
      name: "location",
      value: /Location \u2192 data:Web/
    },
    {
      name: "__proto__.write",
      value: "write()"
    },
  ], { webconsole: hud });
});
