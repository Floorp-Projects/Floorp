/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view correctly re-expands nodes after pauses,
 * with the caveat that there are no ignored items in the hierarchy.
 */

const TAB_URL = EXAMPLE_URL + "doc_with-frame.html";

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(4);

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
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
        line: 21
      });
    }

    function pauseDebuggee() {
      generateMouseClickInTab(gTab, "content.document.querySelector('button')");

      // The first 'with' scope should be expanded by default, but the
      // variables haven't been fetched yet. This is how 'with' scopes work.
      return promise.all([
        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES),
        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_VARIABLES)
      ]);
    }

    function stepInDebuggee() {
      // Spin the event loop before causing the debuggee to pause, to allow
      // this function to return first.
      executeSoon(() => {
        EventUtils.sendMouseEvent({ type: "mousedown" },
                                  gDebugger.document.querySelector("#step-in"),
                                  gDebugger);
      });

      return promise.all([
        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES, 1),
        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_VARIABLES, 3),
        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 4),
      ]);
    }

    function testVariablesExpand() {
      let localScope = gVariables.getScopeAtIndex(0);
      let withScope = gVariables.getScopeAtIndex(1);
      let functionScope = gVariables.getScopeAtIndex(2);
      let globalLexicalScope = gVariables.getScopeAtIndex(3);
      let globalScope = gVariables.getScopeAtIndex(4);

      let thisVar = localScope.get("this");
      let windowVar = thisVar.get("window");
      let documentVar = windowVar.get("document");
      let locationVar = documentVar.get("location");

      is(localScope.target.querySelector(".arrow").hasAttribute("open"), true,
         "The localScope arrow should still be expanded.");
      is(withScope.target.querySelector(".arrow").hasAttribute("open"), true,
         "The withScope arrow should still be expanded.");
      is(functionScope.target.querySelector(".arrow").hasAttribute("open"), true,
         "The functionScope arrow should still be expanded.");
      is(globalLexicalScope.target.querySelector(".arrow").hasAttribute("open"), true,
         "The globalLexicalScope arrow should still be expanded.");
      is(globalScope.target.querySelector(".arrow").hasAttribute("open"), true,
         "The globalScope arrow should still be expanded.");
      is(thisVar.target.querySelector(".arrow").hasAttribute("open"), true,
         "The thisVar arrow should still be expanded.");
      is(windowVar.target.querySelector(".arrow").hasAttribute("open"), true,
         "The windowVar arrow should still be expanded.");
      is(documentVar.target.querySelector(".arrow").hasAttribute("open"), true,
         "The documentVar arrow should still be expanded.");
      is(locationVar.target.querySelector(".arrow").hasAttribute("open"), true,
         "The locationVar arrow should still be expanded.");

      is(localScope.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
         "The localScope enumerables should still be expanded.");
      is(withScope.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
         "The withScope enumerables should still be expanded.");
      is(functionScope.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
         "The functionScope enumerables should still be expanded.");
      is(globalLexicalScope.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
         "The globalLexicalScope enumerables should still be expanded.");
      is(globalScope.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
         "The globalScope enumerables should still be expanded.");
      is(thisVar.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
         "The thisVar enumerables should still be expanded.");
      is(windowVar.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
         "The windowVar enumerables should still be expanded.");
      is(documentVar.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
         "The documentVar enumerables should still be expanded.");
      is(locationVar.target.querySelector(".variables-view-element-details").hasAttribute("open"), true,
         "The locationVar enumerables should still be expanded.");

      is(localScope.expanded, true,
         "The localScope expanded getter should return true.");
      is(withScope.expanded, true,
         "The withScope expanded getter should return true.");
      is(functionScope.expanded, true,
         "The functionScope expanded getter should return true.");
      is(globalLexicalScope.expanded, true,
         "The globalLexicalScope expanded getter should return true.");
      is(globalScope.expanded, true,
         "The globalScope expanded getter should return true.");
      is(thisVar.expanded, true,
         "The thisVar expanded getter should return true.");
      is(windowVar.expanded, true,
         "The windowVar expanded getter should return true.");
      is(documentVar.expanded, true,
         "The documentVar expanded getter should return true.");
      is(locationVar.expanded, true,
         "The locationVar expanded getter should return true.");
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

        let thisVar = localScope.get("this");

        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 1).then(() => {
          let windowVar = thisVar.get("window");

          waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 1).then(() => {
            let documentVar = windowVar.get("document");

            waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 1).then(() => {
              let locationVar = documentVar.get("location");

              waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 1).then(() => {
                is(thisVar.expanded, true,
                   "The local scope 'this' should be expanded.");
                is(windowVar.expanded, true,
                   "The local scope 'this.window' should be expanded.");
                is(documentVar.expanded, true,
                   "The local scope 'this.window.document' should be expanded.");
                is(locationVar.expanded, true,
                   "The local scope 'this.window.document.location' should be expanded.");

                deferred.resolve();
              });

              locationVar.expand();
            });

            documentVar.expand();
          });

          windowVar.expand();
        });

        thisVar.expand();
      });

      withScope.expand();
      functionScope.expand();
      globalLexicalScope.expand();
      globalScope.expand();

      return deferred.promise;
    }

    Task.spawn(function* () {
      yield waitForSourceShown(gPanel, ".html");

      yield addBreakpoint();
      yield ensureThreadClientState(gPanel, "resumed");
      yield pauseDebuggee();
      yield prepareVariablesAndProperties();
      yield stepInDebuggee();
      yield testVariablesExpand();
      resumeDebuggerThenCloseAndFinish(gPanel);
    });
  });
}
