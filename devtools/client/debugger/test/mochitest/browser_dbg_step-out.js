/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that stepping out of a function displays the right return value.
 */

const TAB_URL = EXAMPLE_URL + "doc_step-out.html";

var gTab, gPanel, gDebugger;
var gVars;

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gVars = gDebugger.DebuggerView.Variables;

    testNormalReturn();
  });
}

function testNormalReturn() {
  waitForCaretAndScopes(gPanel, 17).then(() => {
    waitForCaretAndScopes(gPanel, 20).then(() => {
      let innerScope = gVars.getScopeAtIndex(0);
      let returnVar = innerScope.get("<return>");

      is(returnVar.name, "<return>",
        "Should have the right property name for the returned value.");
      is(returnVar.value, 10,
        "Should have the right property value for the returned value.");
      ok(returnVar._internalItem, "Should be an internal item");
      ok(returnVar._target.hasAttribute("pseudo-item"),
         "Element should be marked as a pseudo-item");

      resumeDebuggee().then(() => testReturnWithException());
    });

    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("step-out"),
      gDebugger);
  });

  generateMouseClickInTab(gTab, "content.document.getElementById('return')");
}

function testReturnWithException() {
  waitForCaretAndScopes(gPanel, 24).then(() => {
    waitForCaretAndScopes(gPanel, 26).then(() => {
      let innerScope = gVars.getScopeAtIndex(0);
      let exceptionVar = innerScope.get("<exception>");

      is(exceptionVar.name, "<exception>",
        "Should have the right property name for the returned value.");
      is(exceptionVar.value, "boom",
        "Should have the right property value for the returned value.");
      ok(exceptionVar._internalItem, "Should be an internal item");
      ok(exceptionVar._target.hasAttribute("pseudo-item"),
         "Element should be marked as a pseudo-item");

      resumeDebuggee().then(() => closeDebuggerAndFinish(gPanel));
    });

    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("step-out"),
      gDebugger);
  });

  generateMouseClickInTab(gTab, "content.document.getElementById('throw')");
}

function resumeDebuggee() {
  let deferred = promise.defer();
  gDebugger.gThreadClient.resume(deferred.resolve);
  return deferred.promise;
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gVars = null;
});
