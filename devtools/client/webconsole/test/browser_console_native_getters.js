/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check that native getters and setters for DOM elements work as expected in
// variables view - bug 870220.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<title>bug870220</title>\n" +
                 "<p>hello world\n<p>native getters!";

add_task(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  let jsterm = hud.jsterm;

  jsterm.execute("document");

  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "HTMLDocument \u2192 data:text/html;charset=utf8",
      category: CATEGORY_OUTPUT,
      objects: true,
    }],
  });

  let clickable = result.clickableElements[0];
  ok(clickable, "clickable object found");

  executeSoon(() => {
    EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow);
  });

  let fetchedVar = yield jsterm.once("variablesview-fetched");

  let variablesView = fetchedVar._variablesView;
  ok(variablesView, "variables view object");

  let results = yield findVariableViewProperties(fetchedVar, [
    { name: "title", value: "bug870220" },
    { name: "bgColor" },
  ], { webconsole: hud });

  let prop = results[1].matchedProp;
  ok(prop, "matched the |bgColor| property in the variables view");

  // Check that property value updates work.
  let updatedVar = yield updateVariablesViewProperty({
    property: prop,
    field: "value",
    string: "'red'",
    webconsole: hud,
  });

  info("on fetch after background update");

  jsterm.clearOutput(true);
  jsterm.execute("document.bgColor");

  [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "red",
      category: CATEGORY_OUTPUT,
    }],
  });

  yield findVariableViewProperties(updatedVar, [
    { name: "bgColor", value: "red" },
  ], { webconsole: hud });

  jsterm.execute("$$('p')");

  [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "Array [",
      category: CATEGORY_OUTPUT,
      objects: true,
    }],
  });

  clickable = result.clickableElements[0];
  ok(clickable, "clickable object found");

  executeSoon(() => {
    EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow);
  });

  fetchedVar = yield jsterm.once("variablesview-fetched");

  yield findVariableViewProperties(fetchedVar, [
    { name: "0.textContent", value: /hello world/ },
    { name: "1.textContent", value: /native getters/ },
  ], { webconsole: hud });
});
