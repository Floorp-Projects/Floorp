/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the output for console.dir() works even if Logging filter is off.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test for bug 862916";

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  ok(hud, "web console opened");

  hud.setFilterState("log", false);
  registerCleanupFunction(() => hud.setFilterState("log", true));

  hud.jsterm.execute("window.fooBarz = 'bug862916'; " +
                     "console.dir(window)");

  let varView = yield hud.jsterm.once("variablesview-fetched");
  ok(varView, "variables view object");

  yield findVariableViewProperties(varView, [
    { name: "fooBarz", value: "bug862916" },
  ], { webconsole: hud });
});

