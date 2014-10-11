/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view correctly filters nodes by name.
 */

const TAB_URL = EXAMPLE_URL + "doc_with-frame.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gVariables, gSearchBox;

function test() {
  // Debug test slaves are quite slow at this test.
  requestLongerTimeout(4);

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gVariables = gDebugger.DebuggerView.Variables;

    gVariables._enableSearch();
    gSearchBox = gVariables._searchboxNode;

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

    EventUtils.sendMouseEvent({ type: "click" },
      gDebuggee.document.querySelector("button"),
      gDebuggee);
  });
}

function testVariablesAndPropertiesFiltering() {
  let localScope = gVariables.getScopeAtIndex(0);
  let withScope = gVariables.getScopeAtIndex(1);
  let functionScope = gVariables.getScopeAtIndex(2);
  let globalScope = gVariables.getScopeAtIndex(3);
  let protoVar = localScope.get("__proto__");
  let constrVar = protoVar.get("constructor");
  let proto2Var = constrVar.get("__proto__");
  let constr2Var = proto2Var.get("constructor");

  function testFiltered() {
    is(localScope.expanded, true,
      "The localScope should be expanded.");
    is(withScope.expanded, true,
      "The withScope should be expanded.");
    is(functionScope.expanded, true,
      "The functionScope should be expanded.");
    is(globalScope.expanded, true,
      "The globalScope should be expanded.");

    is(protoVar.expanded, true,
      "The protoVar should be expanded.");
    is(constrVar.expanded, true,
      "The constrVar should be expanded.");
    is(proto2Var.expanded, true,
      "The proto2Var should be expanded.");
    is(constr2Var.expanded, true,
      "The constr2Var should be expanded.");

    is(localScope.target.querySelectorAll(".variables-view-variable:not([unmatched])").length, 1,
      "There should be 1 variable displayed in the local scope.");
    is(withScope.target.querySelectorAll(".variables-view-variable:not([unmatched])").length, 0,
      "There should be 0 variables displayed in the with scope.");
    is(functionScope.target.querySelectorAll(".variables-view-variable:not([unmatched])").length, 0,
      "There should be 0 variables displayed in the function scope.");
    is(globalScope.target.querySelectorAll(".variables-view-variable:not([unmatched])").length, 0,
      "There should be 0 variables displayed in the global scope.");

    is(withScope.target.querySelectorAll(".variables-view-property:not([unmatched])").length, 0,
      "There should be 0 properties displayed in the with scope.");
    is(functionScope.target.querySelectorAll(".variables-view-property:not([unmatched])").length, 0,
      "There should be 0 properties displayed in the function scope.");
    is(globalScope.target.querySelectorAll(".variables-view-property:not([unmatched])").length, 0,
      "There should be 0 properties displayed in the global scope.");

    is(localScope.target.querySelectorAll(".variables-view-variable:not([unmatched]) > .title > .name")[0].getAttribute("value"),
      "__proto__", "The only inner variable displayed should be '__proto__'");
    is(localScope.target.querySelectorAll(".variables-view-property:not([unmatched]) > .title > .name")[0].getAttribute("value"),
      "constructor", "The first inner property displayed should be 'constructor'");
    is(localScope.target.querySelectorAll(".variables-view-property:not([unmatched]) > .title > .name")[1].getAttribute("value"),
      "__proto__", "The second inner property displayed should be '__proto__'");
    is(localScope.target.querySelectorAll(".variables-view-property:not([unmatched]) > .title > .name")[2].getAttribute("value"),
      "constructor", "The third inner property displayed should be 'constructor'");
  }

  function firstFilter() {
    typeText(gSearchBox, "constructor");
    testFiltered();
  }

  function secondFilter() {
    localScope.collapse();
    withScope.collapse();
    functionScope.collapse();
    globalScope.collapse();
    protoVar.collapse();
    constrVar.collapse();
    proto2Var.collapse();
    constr2Var.collapse();

    is(localScope.expanded, false,
      "The localScope should not be expanded.");
    is(withScope.expanded, false,
      "The withScope should not be expanded.");
    is(functionScope.expanded, false,
      "The functionScope should not be expanded.");
    is(globalScope.expanded, false,
      "The globalScope should not be expanded.");

    is(protoVar.expanded, false,
      "The protoVar should not be expanded.");
    is(constrVar.expanded, false,
      "The constrVar should not be expanded.");
    is(proto2Var.expanded, false,
      "The proto2Var should not be expanded.");
    is(constr2Var.expanded, false,
      "The constr2Var should not be expanded.");

    clearText(gSearchBox);
    typeText(gSearchBox, "constructor");
    testFiltered();
  }

  firstFilter();
  secondFilter();
}

function prepareVariablesAndProperties() {
  let deferred = promise.defer();

  let localScope = gVariables.getScopeAtIndex(0);
  let withScope = gVariables.getScopeAtIndex(1);
  let functionScope = gVariables.getScopeAtIndex(2);
  let globalScope = gVariables.getScopeAtIndex(3);

  is(localScope.expanded, true,
    "The localScope should be expanded.");
  is(withScope.expanded, false,
    "The withScope should not be expanded yet.");
  is(functionScope.expanded, false,
    "The functionScope should not be expanded yet.");
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
    is(globalScope.expanded, true,
      "The globalScope should now be expanded.");

    let protoVar = localScope.get("__proto__");

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 1).then(() => {
      let constrVar = protoVar.get("constructor");

      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 1).then(() => {
        let proto2Var = constrVar.get("__proto__");

        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 1).then(() => {
          let constr2Var = proto2Var.get("constructor");

          waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 1).then(() => {
            is(protoVar.expanded, true,
              "The local scope '__proto__' should be expanded.");
            is(constrVar.expanded, true,
              "The local scope '__proto__.constructor' should be expanded.");
            is(proto2Var.expanded, true,
              "The local scope '__proto__.constructor.__proto__' should be expanded.");
            is(constr2Var.expanded, true,
              "The local scope '__proto__.constructor.__proto__.constructor' should be expanded.");

            deferred.resolve();
          });

          constr2Var.expand();
        });

        proto2Var.expand();
      });

      constrVar.expand();
    });

    protoVar.expand();
  });

  withScope.expand();
  functionScope.expand();
  globalScope.expand();

  return deferred.promise;
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gVariables = null;
  gSearchBox = null;
});
