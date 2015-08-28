/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view correctly re-expands *scopes* after pauses.
 */

const TAB_URL = EXAMPLE_URL + "doc_scope-variable-4.html";

let gTab, gPanel, gDebugger;
let gBreakpoints, gSources, gVariables;

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(4);

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    gSources = gDebugger.DebuggerView.Sources;
    gVariables = gDebugger.DebuggerView.Variables;

    // Always expand all items between pauses.
    gVariables.commitHierarchyIgnoredItems = Object.create(null);

    waitForSourceShown(gPanel, ".html")
      .then(addBreakpoint)
      .then(() => ensureThreadClientState(gPanel, "resumed"))
      .then(pauseDebuggee)
      .then(prepareScopes)
      .then(resumeDebuggee)
      .then(testVariablesExpand)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function addBreakpoint() {
  return gBreakpoints.addBreakpoint({ actor: gSources.selectedValue, line: 18 });
}

function pauseDebuggee() {
  callInTab(gTab, "test");

  return waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);
}

function resumeDebuggee() {
  // Spin the event loop before causing the debuggee to pause, to allow
  // this function to return first.
  executeSoon(() => {
    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.querySelector("#resume"),
      gDebugger);
  });

  return waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);
}

function testVariablesExpand() {
  let localScope = gVariables.getScopeAtIndex(0);
  let functionScope = gVariables.getScopeAtIndex(1);
  let globalScope = gVariables.getScopeAtIndex(2);

  is(localScope.target.querySelector(".arrow").hasAttribute("open"), true,
    "The localScope arrow should still be expanded.");
  is(functionScope.target.querySelector(".arrow").hasAttribute("open"), true,
    "The functionScope arrow should still be expanded.");
  is(globalScope.target.querySelector(".arrow").hasAttribute("open"), false,
    "The globalScope arrow should not be expanded.");

  is(localScope.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
    "The localScope enumerables should still be expanded.");
  is(functionScope.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
    "The functionScope enumerables should still be expanded.");
  is(globalScope.target.querySelector(".variables-view-element-details").hasAttribute("open"), false,
    "The globalScope enumerables should not be expanded.");

  is(localScope.expanded, true,
    "The localScope expanded getter should return true.");
  is(functionScope.expanded, true,
    "The functionScope expanded getter should return true.");
  is(globalScope.expanded, false,
    "The globalScope expanded getter should return false.");
}

function prepareScopes() {
  let localScope = gVariables.getScopeAtIndex(0);
  let functionScope = gVariables.getScopeAtIndex(1);
  let globalScope = gVariables.getScopeAtIndex(2);

  is(localScope.expanded, true,
    "The localScope should be expanded.");
  is(functionScope.expanded, false,
    "The functionScope should not be expanded yet.");
  is(globalScope.expanded, false,
    "The globalScope should not be expanded yet.");

  localScope.collapse();
  functionScope.expand();

  // Don't for any events to be triggered, because the Function scope is
  // an environment to which scope arguments and variables are already attached.
  is(localScope.expanded, false,
    "The localScope should not be expanded anymore.");
  is(functionScope.expanded, true,
    "The functionScope should now be expanded.");
  is(globalScope.expanded, false,
    "The globalScope should still not be expanded.");
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gBreakpoints = null;
  gSources = null;
  gVariables = null;
});
