/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that non-enumerable variables and properties can be hidden
 * in the variables view.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

var gTab, gPanel, gDebugger;

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;

    waitForCaretAndScopes(gPanel, 14).then(performTest);
    callInTab(gTab, "simpleCall");
  });
}

function performTest() {
  let testScope = gDebugger.DebuggerView.Variables.addScope("test-scope");
  let testVar = testScope.addItem("foo");

  testVar.addItems({
    foo: {
      value: "bar",
      enumerable: true
    },
    bar: {
      value: "foo",
      enumerable: false
    }
  });

  // Expand the scope and variable.
  testScope.expand();
  testVar.expand();

  // Expanding the non-enumerable container is synchronously dispatched
  // on the main thread, so wait for the next tick.
  executeSoon(() => {
    let details = testVar._enum;
    let nonenum = testVar._nonenum;

    is(details.childNodes.length, 1,
      "There should be just one property in the .details container.");
    ok(details.hasAttribute("open"),
      ".details container should be visible.");
    ok(nonenum.hasAttribute("open"),
      ".nonenum container should be visible.");
    is(nonenum.childNodes.length, 1,
      "There should be just one property in the .nonenum container.");

    // Uncheck 'show hidden properties'.
    gDebugger.DebuggerView.Options._showVariablesOnlyEnumItem.setAttribute("checked", "true");
    gDebugger.DebuggerView.Options._toggleShowVariablesOnlyEnum();

    ok(details.hasAttribute("open"),
      ".details container should stay visible.");
    ok(!nonenum.hasAttribute("open"),
      ".nonenum container should become hidden.");

    // Check 'show hidden properties'.
    gDebugger.DebuggerView.Options._showVariablesOnlyEnumItem.setAttribute("checked", "false");
    gDebugger.DebuggerView.Options._toggleShowVariablesOnlyEnum();

    ok(details.hasAttribute("open"),
      ".details container should stay visible.");
    ok(nonenum.hasAttribute("open"),
      ".nonenum container should become visible.");

    // Collapse the variable. This is done on the current tick.
    testVar.collapse();

    ok(!details.hasAttribute("open"),
      ".details container should be hidden.");
    ok(!nonenum.hasAttribute("open"),
      ".nonenum container should be hidden.");

    // Uncheck 'show hidden properties'.
    gDebugger.DebuggerView.Options._showVariablesOnlyEnumItem.setAttribute("checked", "true");
    gDebugger.DebuggerView.Options._toggleShowVariablesOnlyEnum();

    ok(!details.hasAttribute("open"),
      ".details container should stay hidden.");
    ok(!nonenum.hasAttribute("open"),
      ".nonenum container should stay hidden.");

    // Check 'show hidden properties'.
    gDebugger.DebuggerView.Options._showVariablesOnlyEnumItem.setAttribute("checked", "false");
    gDebugger.DebuggerView.Options._toggleShowVariablesOnlyEnum();

    resumeDebuggerThenCloseAndFinish(gPanel);
  });
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
});
