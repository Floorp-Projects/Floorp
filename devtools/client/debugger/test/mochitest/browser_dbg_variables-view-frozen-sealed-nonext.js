/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test checks that we properly set the frozen, sealed, and non-extensbile
 * attributes on variables so that the F/S/N is shown in the variables view.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

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

    prepareTest();
  });
}

function prepareTest() {
  gDebugger.once(gDebugger.EVENTS.FETCHED_SCOPES, runTest);

  evalInTab(gTab, "(" + function () {
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
       "The variable should not be frozen.");
    ok(!aVariable.hasAttribute("sealed"),
       "The variable should not be sealed.");
    ok(!aVariable.hasAttribute("non-extensible"),
       "The variable should be extensible.");
  };

  let testers = {
    frozen: function (aVariable) {
      ok(aVariable.hasAttribute("frozen"),
        "The variable should be frozen.");
    },
    sealed: function (aVariable) {
      ok(aVariable.hasAttribute("sealed"),
        "The variable should be sealed.");
    },
    nonExtensible: function (aVariable) {
      ok(aVariable.hasAttribute("non-extensible"),
        "The variable should be non-extensible.");
    },
    extensible: hasNoneTester,
    string: hasNoneTester,
    arguments: hasNoneTester,
    this: hasNoneTester
  };

  let variables = gDebugger.document.querySelectorAll(".variable-or-property");

  for (let variable of variables) {
    let name = variable.querySelector(".name").getAttribute("value");
    let tester = testers[name];
    delete testers[name];

    ok(tester, "We should have a tester for the '" + name + "' variable.");
    tester(variable);
  }

  is(Object.keys(testers).length, 0,
    "We should have run and removed all the testers.");

  resumeDebuggerThenCloseAndFinish(gPanel);
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
});
