/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that property values are not missing when the property names only contain whitespace.
 */

const TAB_URL = EXAMPLE_URL + "doc_whitespace-property-names.html";

var test = Task.async(function* () {
  let options = {
    source: TAB_URL,
    line: 1
  };
  var dbg = initDebugger(TAB_URL, options);
  const [tab,, panel] = yield dbg;
  const debuggerLineNumber = 24;
  const scopes = waitForCaretAndScopes(panel, debuggerLineNumber);
  callInTab(tab, "doPause");
  yield scopes;

  const variables = panel.panelWin.DebuggerView.Variables;
  ok(variables, "Should get the variables view.");

  const scope = [...variables][0];
  ok(scope, "Should get the current function's scope.");

  let obj;
  for (let [name, value] of scope) {
    if (name === "obj") {
      obj = value;
    }
  }
  ok(obj, "Should have found the 'obj' variable");

  info("Expanding variable 'obj'");
  let expanded = once(variables, "fetched");
  obj.expand();
  yield expanded;

  let values = [" ", "\r", "\n", "\t", "\f", "\uFEFF", "\xA0"];
  let count = values.length;

  for (let [property, value] of obj) {
    let index = values.indexOf(property);
    if (index >= 0) {
      --count;
      is(value._nameString, property,
         "The _nameString is different than the property name");
      is(value._valueString, index + "",
         "The _valueString is different than the stringified value");
      is(value._valueLabel.getAttribute("value"), index + "",
         "The _valueLabel value is different than the stringified value");
    }
  }
  is(count, 0, "There are " + count + " missing properties");

  resumeDebuggerThenCloseAndFinish(panel);
});
