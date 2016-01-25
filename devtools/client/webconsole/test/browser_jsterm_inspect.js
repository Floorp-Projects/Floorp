/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check that the inspect() jsterm helper function works.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>hello bug 869981";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();
  let jsterm = hud.jsterm;

  jsterm.execute("testProp = 'testValue'");

  let fetched = jsterm.once("variablesview-fetched");
  jsterm.execute("inspect(window)");
  let variable = yield fetched;

  ok(variable._variablesView, "variables view object");

  yield findVariableViewProperties(variable, [
    { name: "testProp", value: "testValue" },
    { name: "document", value: /HTMLDocument \u2192 data:/ },
  ], { webconsole: hud });
});
