/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view correctly shows/hides nodes when various
 * keyboard shortcuts are pressed in the debugger's searchbox.
 */

const TAB_URL = EXAMPLE_URL + "doc_with-frame.html";

var gTab, gPanel, gDebugger;
var gEditor, gVariables, gSearchBox;

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
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
    function() {
      assertExpansion([true, false, false, false, false]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    function() {
      assertExpansion([true, false, false, false, false]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    function() {
      assertExpansion([true, false, false, false, false]);
      gEditor.focus();
    },
    function() {
      assertExpansion([true, false, false, false, false]);
      typeText(gSearchBox, "*");
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      gEditor.focus();
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      backspaceText(gSearchBox, 1);
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      gEditor.focus();
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      localScope.collapse();
      withScope.collapse();
      functionScope.collapse();
      globalLexicalScope.collapse();
      globalScope.collapse();
    },
    function() {
      assertExpansion([false, false, false, false, false]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    function() {
      assertExpansion([false, false, false, false, false]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    function() {
      assertExpansion([false, false, false, false, false]);
      gEditor.focus();
    },
    function() {
      assertExpansion([false, false, false, false, false]);
      clearText(gSearchBox);
      typeText(gSearchBox, "*");
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      gEditor.focus();
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      backspaceText(gSearchBox, 1);
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      EventUtils.sendKey("RETURN", gDebugger);
    },
    function() {
      assertExpansion([true, true, true, true, true]);
      gEditor.focus();
    },
    function() {
      assertExpansion([true, true, true, true, true]);
    }
  ];

  function assertExpansion(aFlags) {
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
    "The globalLexicalScope should not be expanded yet.");
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
      "The globalLexicalScope should now be expanded.");
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

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gVariables = null;
  gSearchBox = null;
});
