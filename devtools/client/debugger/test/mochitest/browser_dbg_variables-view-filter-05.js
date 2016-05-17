/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view correctly shows/hides nodes when various
 * keyboard shortcuts are pressed in the debugger's searchbox.
 */

const TAB_URL = EXAMPLE_URL + "doc_with-frame.html";

var gTab, gPanel, gDebugger;
var gVariables, gSearchBox;

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gVariables = gDebugger.DebuggerView.Variables;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    // The first 'with' scope should be expanded by default, but the
    // variables haven't been fetched yet. This is how 'with' scopes work.
    promise.all([
      waitForSourceAndCaret(gPanel, ".html", 22),
      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES),
      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_VARIABLES)
    ]).then(prepareVariablesAndProperties)
      .then(testVariablesAndPropertiesFiltering)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    generateMouseClickInTab(gTab, "content.document.querySelector('button')");
  });
}

function testVariablesAndPropertiesFiltering() {
  let localScope = gVariables.getScopeAtIndex(0);
  let withScope = gVariables.getScopeAtIndex(1);
  let functionScope = gVariables.getScopeAtIndex(2);
  let globalLexicalScope = gVariables.getScopeAtIndex(3);
  let globalScope = gVariables.getScopeAtIndex(4);
  let step = 0;

  let tests = [
    function () {
      assertScopeExpansion([true, false, false, false, false]);
      typeText(gSearchBox, "*arguments");
    },
    function () {
      assertScopeExpansion([true, true, true, true, true]);
      assertVariablesCountAtLeast([0, 0, 1, 0, 0]);

      is(functionScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[0].getAttribute("value"),
        "arguments", "The arguments pseudoarray should be visible.");
      is(functionScope.get("arguments").expanded, false,
        "The arguments pseudoarray in functionScope should not be expanded.");

      backspaceText(gSearchBox, 6);
    },
    function () {
      assertScopeExpansion([true, true, true, true, true]);
      assertVariablesCountAtLeast([0, 0, 1, 0, 1]);

      is(functionScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[0].getAttribute("value"),
        "arguments", "The arguments pseudoarray should be visible.");
      is(functionScope.get("arguments").expanded, false,
        "The arguments pseudoarray in functionScope should not be expanded.");

      is(globalScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[0].getAttribute("value"),
        "EventTarget", "The EventTarget object should be visible.");
      is(globalScope.get("EventTarget").expanded, false,
        "The EventTarget object in globalScope should not be expanded.");

      backspaceText(gSearchBox, 2);
    },
    function () {
      assertScopeExpansion([true, true, true, true, true]);
      assertVariablesCountAtLeast([0, 1, 3, 0, 1]);

      is(functionScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[0].getAttribute("value"),
        "aNumber", "The aNumber param should be visible.");
      is(functionScope.get("aNumber").expanded, false,
        "The aNumber param in functionScope should not be expanded.");

      is(functionScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[1].getAttribute("value"),
        "a", "The a variable should be visible.");
      is(functionScope.get("a").expanded, false,
        "The a variable in functionScope should not be expanded.");

      is(functionScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[2].getAttribute("value"),
        "arguments", "The arguments pseudoarray should be visible.");
      is(functionScope.get("arguments").expanded, false,
        "The arguments pseudoarray in functionScope should not be expanded.");

      backspaceText(gSearchBox, 1);
    },
    function () {
      assertScopeExpansion([true, true, true, true, true]);
      assertVariablesCountAtLeast([4, 1, 3, 0, 1]);

      is(localScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[0].getAttribute("value"),
        "this", "The this reference should be visible.");
      is(localScope.get("this").expanded, false,
        "The this reference in localScope should not be expanded.");

      is(localScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[1].getAttribute("value"),
        "alpha", "The alpha variable should be visible.");
      is(localScope.get("alpha").expanded, false,
        "The alpha variable in localScope should not be expanded.");

      is(localScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[2].getAttribute("value"),
        "beta", "The beta variable should be visible.");
      is(localScope.get("beta").expanded, false,
        "The beta variable in localScope should not be expanded.");

      is(localScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[3].getAttribute("value"),
        "__proto__", "The __proto__ reference should be visible.");
      is(localScope.get("__proto__").expanded, false,
        "The __proto__ reference in localScope should not be expanded.");

      is(functionScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[0].getAttribute("value"),
        "aNumber", "The aNumber param should be visible.");
      is(functionScope.get("aNumber").expanded, false,
        "The aNumber param in functionScope should not be expanded.");

      is(functionScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[1].getAttribute("value"),
        "a", "The a variable should be visible.");
      is(functionScope.get("a").expanded, false,
        "The a variable in functionScope should not be expanded.");

      is(functionScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[2].getAttribute("value"),
        "arguments", "The arguments pseudoarray should be visible.");
      is(functionScope.get("arguments").expanded, false,
        "The arguments pseudoarray in functionScope should not be expanded.");
    }
  ];

  function assertScopeExpansion(aFlags) {
    is(localScope.expanded, aFlags[0],
      "The localScope should " + (aFlags[0] ? "" : "not ") +
       "be expanded at this point (" + step + ").");

    is(withScope.expanded, aFlags[1],
      "The withScope should " + (aFlags[1] ? "" : "not ") +
       "be expanded at this point (" + step + ").");

    is(functionScope.expanded, aFlags[2],
      "The functionScope should " + (aFlags[2] ? "" : "not ") +
       "be expanded at this point (" + step + ").");

    is(globalLexicalScope.expanded, aFlags[3],
      "The globalLexicalScope should " + (aFlags[3] ? "" : "not ") +
       "be expanded at this point (" + step + ").");

    is(globalScope.expanded, aFlags[4],
      "The globalScope should " + (aFlags[4] ? "" : "not ") +
       "be expanded at this point (" + step + ").");
  }

  function assertVariablesCountAtLeast(aCounts) {
    ok(localScope.target.querySelectorAll(".variables-view-variable:not([unmatched])").length >= aCounts[0],
      "There should be " + aCounts[0] +
      " variable displayed in the local scope (" + step + ").");

    ok(withScope.target.querySelectorAll(".variables-view-variable:not([unmatched])").length >= aCounts[1],
      "There should be " + aCounts[1] +
      " variable displayed in the with scope (" + step + ").");

    ok(functionScope.target.querySelectorAll(".variables-view-variable:not([unmatched])").length >= aCounts[2],
      "There should be " + aCounts[2] +
      " variable displayed in the function scope (" + step + ").");

    ok(globalLexicalScope.target.querySelectorAll(".variables-view-variable:not([unmatched])").length >= aCounts[3],
      "There should be " + aCounts[3] +
       " variable displayed in the global scope (" + step + ").");

    ok(globalScope.target.querySelectorAll(".variables-view-variable:not([unmatched])").length >= aCounts[4],
      "There should be " + aCounts[4] +
      " variable displayed in the global scope (" + step + ").");

    step++;
  }

  return promise.all(tests.map(f => f()));
}

function prepareVariablesAndProperties() {
  let deferred = promise.defer();

  let localScope = gVariables.getScopeAtIndex(0);
  let withScope = gVariables.getScopeAtIndex(1);
  let functionScope = gVariables.getScopeAtIndex(2);
  let globalLexicalScope = gVariables.getScopeAtIndex(3);
  let globalScope = gVariables.getScopeAtIndex(4);

  is(localScope.expanded, true,
    "The localScope should be expanded.");
  is(withScope.expanded, false,
    "The withScope should not be expanded yet.");
  is(functionScope.expanded, false,
    "The functionScope should not be expanded yet.");
  is(globalLexicalScope.expanded, false,
    "The globalScope should not be expanded yet.");
  is(globalScope.expanded, false,
    "The globalScope should not be expanded yet.");

  // Wait for only two events to be triggered, because the Function scope is
  // an environment to which scope arguments and variables are already attached.
  waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_VARIABLES, 2).then(() => {
    is(localScope.expanded, true,
      "The localScope should now be expanded.");
    is(withScope.expanded, true,
      "The withScope should now be expanded.");
    is(functionScope.expanded, true,
      "The functionScope should now be expanded.");
    is(globalLexicalScope.expanded, true,
      "The globalScope should now be expanded.");
    is(globalScope.expanded, true,
      "The globalScope should now be expanded.");

    withScope.collapse();
    functionScope.collapse();
    globalLexicalScope.collapse();
    globalScope.collapse();

    deferred.resolve();
  });

  withScope.expand();
  functionScope.expand();
  globalLexicalScope.expand();
  globalScope.expand();

  return deferred.promise;
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gVariables = null;
  gSearchBox = null;
});
