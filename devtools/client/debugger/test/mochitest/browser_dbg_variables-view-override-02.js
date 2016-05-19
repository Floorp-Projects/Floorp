/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that overridden variables in the VariablesView are styled properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_scope-variable-2.html";

function test() {
  Task.spawn(function* () {
    let [tab,, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let events = win.EVENTS;
    let variables = win.DebuggerView.Variables;

    // Wait for the hierarchy to be committed by the VariablesViewController.
    let committedLocalScopeHierarchy = promise.defer();
    variables.oncommit = committedLocalScopeHierarchy.resolve;

    callInTab(tab, "test");
    yield waitForSourceAndCaretAndScopes(panel, ".html", 23);
    yield committedLocalScopeHierarchy.promise;

    let firstScope = variables.getScopeAtIndex(0);
    let secondScope = variables.getScopeAtIndex(1);
    let thirdScope = variables.getScopeAtIndex(2);

    let someVar1 = firstScope.get("a");
    let argsVar1 = firstScope.get("arguments");

    is(someVar1.target.hasAttribute("overridden"), false,
      "The first 'a' variable should not be marked as being overridden.");
    is(argsVar1.target.hasAttribute("overridden"), false,
      "The first 'arguments' variable should not be marked as being overridden.");

    // Wait for the hierarchy to be committed by the VariablesViewController.
    let committedSecondScopeHierarchy = promise.defer();
    variables.oncommit = committedSecondScopeHierarchy.resolve;
    secondScope.expand();
    yield committedSecondScopeHierarchy.promise;

    let someVar2 = secondScope.get("a");
    let argsVar2 = secondScope.get("arguments");

    is(someVar2.target.hasAttribute("overridden"), true,
      "The second 'a' variable should be marked as being overridden.");
    is(argsVar2.target.hasAttribute("overridden"), true,
      "The second 'arguments' variable should be marked as being overridden.");

    // Wait for the hierarchy to be committed by the VariablesViewController.
    let committedThirdScopeHierarchy = promise.defer();
    variables.oncommit = committedThirdScopeHierarchy.resolve;
    thirdScope.expand();
    yield committedThirdScopeHierarchy.promise;

    let someVar3 = thirdScope.get("a");
    let argsVar3 = thirdScope.get("arguments");

    is(someVar3.target.hasAttribute("overridden"), true,
      "The third 'a' variable should be marked as being overridden.");
    is(argsVar3.target.hasAttribute("overridden"), true,
      "The third 'arguments' variable should be marked as being overridden.");

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
