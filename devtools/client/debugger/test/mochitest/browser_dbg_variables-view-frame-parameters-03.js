/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view displays the right variables and
 * properties in the global scope when debugger is paused.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

var gTab, gPanel, gDebugger;
var gVariables;

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gVariables = gDebugger.DebuggerView.Variables;

    waitForCaretAndScopes(gPanel, 24)
      .then(expandGlobalScope)
      .then(testGlobalScope)
      .then(expandWindowVariable)
      .then(testWindowVariable)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .catch(aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    generateMouseClickInTab(gTab, "content.document.querySelector('button')");
  });
}

function expandGlobalScope() {
  let deferred = promise.defer();

  let globalScope = gVariables.getScopeAtIndex(2);
  is(globalScope.expanded, false,
    "The global scope should not be expanded by default.");

  gDebugger.once(gDebugger.EVENTS.FETCHED_VARIABLES, deferred.resolve);

  EventUtils.sendMouseEvent({ type: "mousedown" },
    globalScope.target.querySelector(".name"),
    gDebugger);

  return deferred.promise;
}

function testGlobalScope() {
  let globalScope = gVariables.getScopeAtIndex(2);
  is(globalScope.expanded, true,
    "The global scope should now be expanded.");

  is(globalScope.get("InstallTrigger").target.querySelector(".name").getAttribute("value"), "InstallTrigger",
    "Should have the right property name for 'InstallTrigger'.");
  is(globalScope.get("InstallTrigger").target.querySelector(".value").getAttribute("value"), "InstallTriggerImpl",
    "Should have the right property value for 'InstallTrigger'.");

  is(globalScope.get("SpecialPowers").target.querySelector(".name").getAttribute("value"), "SpecialPowers",
    "Should have the right property name for 'SpecialPowers'.");
  is(globalScope.get("SpecialPowers").target.querySelector(".value").getAttribute("value"), "Object",
    "Should have the right property value for 'SpecialPowers'.");

  is(globalScope.get("window").target.querySelector(".name").getAttribute("value"), "window",
    "Should have the right property name for 'window'.");
  is(globalScope.get("window").target.querySelector(".value").getAttribute("value"),
    "Window \u2192 doc_frame-parameters.html",
    "Should have the right property value for 'window'.");

  is(globalScope.get("document").target.querySelector(".name").getAttribute("value"), "document",
    "Should have the right property name for 'document'.");
  is(globalScope.get("document").target.querySelector(".value").getAttribute("value"),
    "HTMLDocument \u2192 doc_frame-parameters.html",
    "Should have the right property value for 'document'.");

  is(globalScope.get("undefined").target.querySelector(".name").getAttribute("value"), "undefined",
    "Should have the right property name for 'undefined'.");
  is(globalScope.get("undefined").target.querySelector(".value").getAttribute("value"), "undefined",
    "Should have the right property value for 'undefined'.");

  is(globalScope.get("undefined").target.querySelector(".enum").childNodes.length, 0,
    "Should have no child enumerable properties for 'undefined'.");
  is(globalScope.get("undefined").target.querySelector(".nonenum").childNodes.length, 0,
    "Should have no child non-enumerable properties for 'undefined'.");
}

function expandWindowVariable() {
  let deferred = promise.defer();

  let windowVar = gVariables.getScopeAtIndex(2).get("window");
  is(windowVar.expanded, false,
    "The window variable should not be expanded by default.");

  gDebugger.once(gDebugger.EVENTS.FETCHED_PROPERTIES, deferred.resolve);

  EventUtils.sendMouseEvent({ type: "mousedown" },
    windowVar.target.querySelector(".name"),
    gDebugger);

  return deferred.promise;
}

function testWindowVariable() {
  let windowVar = gVariables.getScopeAtIndex(2).get("window");
  is(windowVar.expanded, true,
    "The window variable should now be expanded.");

  is(windowVar.get("InstallTrigger").target.querySelector(".name").getAttribute("value"), "InstallTrigger",
    "Should have the right property name for 'InstallTrigger'.");
  is(windowVar.get("InstallTrigger").target.querySelector(".value").getAttribute("value"), "InstallTriggerImpl",
    "Should have the right property value for 'InstallTrigger'.");

  is(windowVar.get("SpecialPowers").target.querySelector(".name").getAttribute("value"), "SpecialPowers",
    "Should have the right property name for 'SpecialPowers'.");
  is(windowVar.get("SpecialPowers").target.querySelector(".value").getAttribute("value"), "Object",
    "Should have the right property value for 'SpecialPowers'.");

  is(windowVar.get("window").target.querySelector(".name").getAttribute("value"), "window",
    "Should have the right property name for 'window'.");
  is(windowVar.get("window").target.querySelector(".value").getAttribute("value"),
    "Window \u2192 doc_frame-parameters.html",
    "Should have the right property value for 'window'.");

  is(windowVar.get("document").target.querySelector(".name").getAttribute("value"), "document",
    "Should have the right property name for 'document'.");
  is(windowVar.get("document").target.querySelector(".value").getAttribute("value"),
    "HTMLDocument \u2192 doc_frame-parameters.html",
    "Should have the right property value for 'document'.");

  is(windowVar.get("undefined").target.querySelector(".name").getAttribute("value"), "undefined",
    "Should have the right property name for 'undefined'.");
  is(windowVar.get("undefined").target.querySelector(".value").getAttribute("value"), "undefined",
    "Should have the right property value for 'undefined'.");

  is(windowVar.get("undefined").target.querySelector(".enum").childNodes.length, 0,
    "Should have no child enumerable properties for 'undefined'.");
  is(windowVar.get("undefined").target.querySelector(".nonenum").childNodes.length, 0,
    "Should have no child non-enumerable properties for 'undefined'.");
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gVariables = null;
});
