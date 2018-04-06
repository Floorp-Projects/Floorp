/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the inspect() jsterm helper function works.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>hello bug 869981";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();
  let jsterm = hud.jsterm;

  /* Check that the window object is inspected */
  jsterm.execute("testProp = 'testValue'");

  let updated = jsterm.once("variablesview-updated");
  jsterm.execute("inspect(window)");
  let view = yield updated;
  ok(view, "variables view object");

  // The single variable view contains a scope with the variable name
  // and unnamed subitem that contains the properties
  let variable = view.getScopeAtIndex(0).get(undefined);
  ok(variable, "variable object");

  yield findVariableViewProperties(variable, [
    { name: "testProp", value: "testValue" },
    { name: "document", value: /HTMLDocument \u2192 data:/ },
  ], { webconsole: hud });

  /* Check that a primitive value can be inspected, too */
  let updated2 = jsterm.once("variablesview-updated");
  jsterm.execute("inspect(1)");
  let view2 = yield updated2;
  ok(view2, "variables view object");

  // Check the label of the scope - it should contain the value
  let scope = view.getScopeAtIndex(0);
  ok(scope, "variable object");

  is(scope.name, "1", "The value of the primitive var is correct");
});
