/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that Promises get their internal state added as psuedo properties.
 */

const TAB_URL = EXAMPLE_URL + "doc_promise.html";

var test = Task.async(function* () {
  let options = {
    source: TAB_URL,
    line: 1
  };
  const [tab,, panel] = yield initDebugger(TAB_URL, options);

  const scopes = waitForCaretAndScopes(panel, 21);
  callInTab(tab, "doPause");
  yield scopes;

  const variables = panel.panelWin.DebuggerView.Variables;
  ok(variables, "Should get the variables view.");

  const scope = [...variables][0];
  ok(scope, "Should get the current function's scope.");

  const promiseVariables = [...scope].filter(([name]) =>
    ["p", "f", "r"].indexOf(name) !== -1);

  is(promiseVariables.length, 3,
     "Should have our 3 promise variables: p, f, r");

  for (let [name, item] of promiseVariables) {
    info("Expanding variable '" + name + "'");
    let expanded = once(variables, "fetched");
    item.expand();
    yield expanded;

    let foundState = false;
    switch (name) {
      case "p":
        for (let [property, { value }] of item) {
          if (property !== "<state>") {
            isnot(property, "<value>",
                  "A pending promise shouldn't have a value");
            isnot(property, "<reason>",
                  "A pending promise shouldn't have a reason");
            continue;
          }

          foundState = true;
          is(value, "pending", "The state should be pending.");
        }
        ok(foundState, "We should have found the <state> property.");
        break;

      case "f":
        let foundValue = false;
        for (let [property, value] of item) {
          if (property === "<state>") {
            foundState = true;
            is(value.value, "fulfilled", "The state should be fulfilled.");
          } else if (property === "<value>") {
            foundValue = true;

            let expanded = once(variables, "fetched");
            value.expand();
            yield expanded;

            let expectedProps = new Map([["a", 1], ["b", 2], ["c", 3]]);
            for (let [prop, val] of value) {
              if (prop === "__proto__") {
                continue;
              }
              ok(expectedProps.has(prop), "The property should be expected.");
              is(val.value, expectedProps.get(prop), "The property value should be correct.");
              expectedProps.delete(prop);
            }
            is(Object.keys(expectedProps).length, 0,
               "Should have found all of the expected properties.");
          } else {
            isnot(property, "<reason>",
                  "A fulfilled promise shouldn't have a reason");
          }
        }
        ok(foundState, "We should have found the <state> property.");
        ok(foundValue, "We should have found the <value> property.");
        break;

      case "r":
        let foundReason = false;
        for (let [property, value] of item) {
          if (property === "<state>") {
            foundState = true;
            is(value.value, "rejected", "The state should be rejected.");
          } else if (property === "<reason>") {
            foundReason = true;

            let expanded = once(variables, "fetched");
            value.expand();
            yield expanded;

            let foundMessage = false;
            for (let [prop, val] of value) {
              if (prop !== "message") {
                continue;
              }
              foundMessage = true;
              is(val.value, "uh oh", "Should have the correct error message.");
            }
            ok(foundMessage, "Should have found the error's message");
          } else {
            isnot(property, "<value>",
                  "A rejected promise shouldn't have a value");
          }
        }
        ok(foundState, "We should have found the <state> property.");
        break;
    }
  }

  resumeDebuggerThenCloseAndFinish(panel);
});
