/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view correctly re-expands *scopes* after pauses.
 */

const TAB_URL = EXAMPLE_URL + "doc_scope-variable-4.html";

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(4);

  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gSources = gDebugger.DebuggerView.Sources;
    const gVariables = gDebugger.DebuggerView.Variables;
    const queries = gDebugger.require("./content/queries");
    const getState = gDebugger.DebuggerController.getState;
    const actions = bindActionCreators(gPanel);

    // Always expand all items between pauses.
    gVariables.commitHierarchyIgnoredItems = Object.create(null);

    function addBreakpoint() {
      return actions.addBreakpoint({
        actor: gSources.selectedValue,
        line: 18
      });
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

    Task.spawn(function* () {
      yield addBreakpoint();
      yield ensureThreadClientState(gPanel, "resumed");
      yield pauseDebuggee();
      yield prepareScopes();
      yield resumeDebuggee();
      yield testVariablesExpand();
      resumeDebuggerThenCloseAndFinish(gPanel);
    });
  });
}
