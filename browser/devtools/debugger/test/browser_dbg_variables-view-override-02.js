/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that overridden variables in the VariablesView are styled properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_scope-variable-2.html";

function test() {
  Task.spawn(function() {
    let [tab, debuggee, panel] = yield initDebugger(TAB_URL);
    let win = panel.panelWin;
    let events = win.EVENTS;
    let variables = win.DebuggerView.Variables;

    // Wait for the hierarchy to be committed by the VariablesViewController.
    let committed = promise.defer();
    variables.oncommit = committed.resolve;

    // Allow this generator function to yield first.
    executeSoon(() => debuggee.test());
    yield waitForSourceAndCaretAndScopes(panel, ".html", 23);
    yield committed.promise;

    let firstScope = variables.getScopeAtIndex(0);
    let secondScope = variables.getScopeAtIndex(1);
    let thirdScope = variables.getScopeAtIndex(2);

    let someVar1 = firstScope.get("a");
    let someVar2 = secondScope.get("a");
    let someVar3 = thirdScope.get("a");

    let argsVar1 = firstScope.get("arguments");
    let argsVar2 = secondScope.get("arguments");
    let argsVar3 = thirdScope.get("arguments");

    is(someVar1.target.hasAttribute("overridden"), false,
      "The first 'a' variable should not be marked as being overridden.");
    is(someVar2.target.hasAttribute("overridden"), true,
      "The second 'a' variable should be marked as being overridden.");
    is(someVar3.target.hasAttribute("overridden"), true,
      "The third 'a' variable should be marked as being overridden.");

    is(argsVar1.target.hasAttribute("overridden"), false,
      "The first 'arguments' variable should not be marked as being overridden.");
    is(argsVar2.target.hasAttribute("overridden"), true,
      "The second 'arguments' variable should be marked as being overridden.");
    is(argsVar3.target.hasAttribute("overridden"), true,
      "The third 'arguments' variable should be marked as being overridden.");

    yield resumeDebuggerThenCloseAndFinish(panel);
  });
}
