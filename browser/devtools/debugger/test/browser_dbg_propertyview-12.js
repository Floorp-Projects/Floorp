/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test checks that we properly set the frozen, sealed, and non-extensbile
// attributes on variables so that the F/S/N is shown in the variables view.

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;

function test() {
  debug_tab_pane(STACK_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    testFSN();
  });
}

function testFSN() {
  gDebugger.addEventListener("Debugger:FetchedVariables", function _onFetchedVariables() {
    gDebugger.removeEventListener("Debugger:FetchedVariables", _onFetchedVariables, false);
    runTest();
  }, false);

  gDebuggee.eval("(" + function () {
    var frozen = Object.freeze({});
    var sealed = Object.seal({});
    var nonExtensible = Object.preventExtensions({});
    var extensible = {};
    var string = "foo bar baz";

    debugger;
  } + "())");
}

function runTest() {
  let hasNoneTester = function (aVariable) {
    ok(!aVariable.hasAttribute("frozen"),
       "The variable should not be frozen");
    ok(!aVariable.hasAttribute("sealed"),
       "The variable should not be sealed");
    ok(!aVariable.hasAttribute("non-extensible"),
       "The variable should be extensible");
  };

  let testers = {
    frozen: function (aVariable) {
      ok(aVariable.hasAttribute("frozen"),
         "The variable should be frozen")
    },
    sealed: function (aVariable) {
      ok(aVariable.hasAttribute("sealed"),
         "The variable should be sealed")
    },
    nonExtensible: function (aVariable) {
      ok(aVariable.hasAttribute("non-extensible"),
         "The variable should be non-extensible")
    },
    extensible: hasNoneTester,
    string: hasNoneTester,
    arguments: hasNoneTester,
    this: hasNoneTester
  };

  let variables = gDebugger.DebuggerView.Variables._parent
    .querySelectorAll(".variable-or-property");

  for (let v of variables) {
    let name = v.querySelector(".name").getAttribute("value");
    let tester = testers[name];
    delete testers[name];
    ok(tester, "We should have a tester for the '" + name + "' variable.");
    tester(v);
  }

  is(Object.keys(testers).length, 0,
     "We should have run and removed all the testers.");

  closeDebuggerAndFinish();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
