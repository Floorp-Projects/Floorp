/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that Map and Set and their Weak friends are displayed in variables view.
 */

"use strict";

const TAB_URL = EXAMPLE_URL + "doc_map-set.html";

var test = Task.async(function* () {
  let options = {
    source: TAB_URL,
    line: 1
  };
  const [tab,, panel] = yield initDebugger(TAB_URL, options);

  const scopes = waitForCaretAndScopes(panel, 37);
  callInTab(tab, "startTest");
  yield scopes;

  const variables = panel.panelWin.DebuggerView.Variables;
  ok(variables, "Should get the variables view.");

  const scope = variables.getScopeAtIndex(0);
  ok(scope, "Should get the current function's scope.");

  /* Test the maps */
  for (let varName of ["map", "weakMap"]) {
    const mapVar = scope.get(varName);
    ok(mapVar, `Retrieved the '${varName}' variable from the scope`);

    info(`Expanding '${varName}' variable`);
    yield mapVar.expand();

    const entries = mapVar.get("<entries>");
    ok(entries, `Retrieved the '${varName}' entries`);

    info(`Expanding '${varName}' entries`);
    yield entries.expand();

    // Check the entries. WeakMap returns its entries in a nondeterministic
    // order, so we make our job easier by not testing the exact values.
    let i = 0;
    for (let [ name, entry ] of entries) {
      is(name, i, `The '${varName}' entry's property name is correct`);
      ok(entry.displayValue.startsWith("Object \u2192 "),
        `The '${varName}' entry's property value is correct`);
      yield entry.expand();

      let key = entry.get("key");
      ok(key, `The '${varName}' entry has the 'key' property`);
      yield key.expand();

      let keyProperty = key.get("a");
      ok(keyProperty,
        `The '${varName}' entry's 'key' has the correct property`);

      let value = entry.get("value");
      ok(value, `The '${varName}' entry has the 'value' property`);

      i++;
    }

    is(i, 2, `The '${varName}' entry count is correct`);

    // Check the extra property on the object
    let extraProp = mapVar.get("extraProp");
    ok(extraProp, `Retrieved the '${varName}' extraProp`);
    is(extraProp.displayValue, "true",
      `The '${varName}' extraProp's value is correct`);
  }

  /* Test the sets */
  for (let varName of ["set", "weakSet"]) {
    const setVar = scope.get(varName);
    ok(setVar, `Retrieved the '${varName}' variable from the scope`);

    info(`Expanding '${varName}' variable`);
    yield setVar.expand();

    const entries = setVar.get("<entries>");
    ok(entries, `Retrieved the '${varName}' entries`);

    info(`Expanding '${varName}' entries`);
    yield entries.expand();

    // Check the entries. WeakSet returns its entries in a nondeterministic
    // order, so we make our job easier by not testing the exact values.
    let i = 0;
    for (let [ name, entry ] of entries) {
      is(name, i, `The '${varName}' entry's property name is correct`);
      is(entry.displayValue, "Object",
        `The '${varName}' entry's property value is correct`);
      yield entry.expand();

      let entryProperty = entry.get("a");
      ok(entryProperty,
        `The '${varName}' entry's value has the correct property`);

      i++;
    }

    is(i, 2, `The '${varName}' entry count is correct`);

    // Check the extra property on the object
    let extraProp = setVar.get("extraProp");
    ok(extraProp, `Retrieved the '${varName}' extraProp`);
    is(extraProp.displayValue, "true",
      `The '${varName}' extraProp's value is correct`);
  }

  resumeDebuggerThenCloseAndFinish(panel);
});
