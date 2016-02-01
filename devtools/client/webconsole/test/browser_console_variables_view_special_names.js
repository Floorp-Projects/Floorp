/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that variables view handles special names like "<return>"
// properly for ordinary displays.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test for bug 1084430";

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  ok(hud, "web console opened");

  hud.setFilterState("log", false);
  registerCleanupFunction(() => hud.setFilterState("log", true));

  hud.jsterm.execute("inspect({ '<return>': 47, '<exception>': 91 })");

  let varView = yield hud.jsterm.once("variablesview-fetched");
  ok(varView, "variables view object");

  let props = yield findVariableViewProperties(varView, [
    { name: "<return>", value: 47 },
    { name: "<exception>", value: 91 },
  ], { webconsole: hud });

  for (let prop of props) {
    ok(!prop.matchedProp._internalItem, prop.name + " is not marked internal");
    let target = prop.matchedProp._target;
    ok(!target.hasAttribute("pseudo-item"),
       prop.name + " is not a pseudo-item");
  }
});
