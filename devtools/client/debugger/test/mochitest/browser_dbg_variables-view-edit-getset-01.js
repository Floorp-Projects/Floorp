/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view knows how to edit getters and setters.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

var gTab, gPanel, gDebugger;
var gL10N, gEditor, gVars, gWatch;

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gL10N = gDebugger.L10N;
    gEditor = gDebugger.DebuggerView.editor;
    gVars = gDebugger.DebuggerView.Variables;
    gWatch = gDebugger.DebuggerView.WatchExpressions;

    gVars.switch = function() {};
    gVars.delete = function() {};

    waitForSourceAndCaretAndScopes(gPanel, ".html", 24)
      .then(() => addWatchExpressions())
      .then(() => testEdit("set", "this._prop = value + ' BEER CAN'", {
        "myVar.prop": "xlerb BEER CAN",
        "myVar.prop + 42": "xlerb BEER CAN42",
        "myVar.prop = 'xlerb'": "xlerb"
      }))
      .then(() => testEdit("set", "{ this._prop = value + ' BEACON' }", {
        "myVar.prop": "xlerb BEACON",
        "myVar.prop + 42": "xlerb BEACON42",
        "myVar.prop = 'xlerb'": "xlerb"
      }))
      .then(() => testEdit("set", "{ this._prop = value + ' BEACON;'; }", {
        "myVar.prop": "xlerb BEACON;",
        "myVar.prop + 42": "xlerb BEACON;42",
        "myVar.prop = 'xlerb'": "xlerb"
      }))
      .then(() => testEdit("set", "{ return this._prop = value + ' BEACON;;'; }", {
        "myVar.prop": "xlerb BEACON;;",
        "myVar.prop + 42": "xlerb BEACON;;42",
        "myVar.prop = 'xlerb'": "xlerb"
      }))
      .then(() => testEdit("set", "function(value) { this._prop = value + ' BACON' }", {
        "myVar.prop": "xlerb BACON",
        "myVar.prop + 42": "xlerb BACON42",
        "myVar.prop = 'xlerb'": "xlerb"
      }))
      .then(() => testEdit("get", "'brelx BEER CAN'", {
        "myVar.prop": "brelx BEER CAN",
        "myVar.prop + 42": "brelx BEER CAN42",
        "myVar.prop = 'xlerb'": "xlerb"
      }))
      .then(() => testEdit("get", "{ 'brelx BEACON' }", {
        "myVar.prop": undefined,
        "myVar.prop + 42": NaN,
        "myVar.prop = 'xlerb'": "xlerb"
      }))
      .then(() => testEdit("get", "{ 'brelx BEACON;'; }", {
        "myVar.prop": undefined,
        "myVar.prop + 42": NaN,
        "myVar.prop = 'xlerb'": "xlerb"
      }))
      .then(() => testEdit("get", "{ return 'brelx BEACON;;'; }", {
        "myVar.prop": "brelx BEACON;;",
        "myVar.prop + 42": "brelx BEACON;;42",
        "myVar.prop = 'xlerb'": "xlerb"
      }))
      .then(() => testEdit("get", "function() { return 'brelx BACON'; }", {
        "myVar.prop": "brelx BACON",
        "myVar.prop + 42": "brelx BACON42",
        "myVar.prop = 'xlerb'": "xlerb"
      }))
      .then(() => testEdit("get", "bogus", {
        "myVar.prop": "ReferenceError: bogus is not defined",
        "myVar.prop + 42": "ReferenceError: bogus is not defined",
        "myVar.prop = 'xlerb'": "xlerb"
      }))
      .then(() => testEdit("set", "sugob", {
        "myVar.prop": "ReferenceError: bogus is not defined",
        "myVar.prop + 42": "ReferenceError: bogus is not defined",
        "myVar.prop = 'xlerb'": "ReferenceError: sugob is not defined"
      }))
      .then(() => testEdit("get", "", {
        "myVar.prop": undefined,
        "myVar.prop + 42": NaN,
        "myVar.prop = 'xlerb'": "ReferenceError: sugob is not defined"
      }))
      .then(() => testEdit("set", "", {
        "myVar.prop": "xlerb",
        "myVar.prop + 42": NaN,
        "myVar.prop = 'xlerb'": "xlerb"
      }))
      .then(() => deleteWatchExpression("myVar.prop = 'xlerb'"))
      .then(() => testEdit("self", "2507", {
        "myVar.prop": 2507,
        "myVar.prop + 42": 2549
      }))
      .then(() => deleteWatchExpression("myVar.prop + 42"))
      .then(() => testEdit("self", "0910", {
        "myVar.prop": 910
      }))
      .then(() => deleteLastWatchExpression("myVar.prop"))
      .then(() => testWatchExpressionsRemoved())
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    generateMouseClickInTab(gTab, "content.document.querySelector('button')");
  });
}

function addWatchExpressions() {
  return promise.resolve(null)
    .then(() => {
      let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS);
      gWatch.addExpression("myVar.prop");
      gEditor.focus();
      return finished;
    })
    .then(() => {
      let exprScope = gVars.getScopeAtIndex(0);
      ok(exprScope,
        "There should be a wach expressions scope in the variables view.");
      is(exprScope.name, gL10N.getStr("watchExpressionsScopeLabel"),
        "The scope's name should be marked as 'Watch Expressions'.");
      is(exprScope._store.size, 1,
        "There should be 1 evaluation available.");

      let w1 = exprScope.get("myVar.prop");
      let w2 = exprScope.get("myVar.prop + 42");
      let w3 = exprScope.get("myVar.prop = 'xlerb'");

      ok(w1, "The first watch expression should be present in the scope.");
      ok(!w2, "The second watch expression should not be present in the scope.");
      ok(!w3, "The third watch expression should not be present in the scope.");

      is(w1.value, 42, "The first value is correct.");
    })
    .then(() => {
      let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS);
      gWatch.addExpression("myVar.prop + 42");
      gEditor.focus();
      return finished;
    })
    .then(() => {
      let exprScope = gVars.getScopeAtIndex(0);
      ok(exprScope,
        "There should be a wach expressions scope in the variables view.");
      is(exprScope.name, gL10N.getStr("watchExpressionsScopeLabel"),
        "The scope's name should be marked as 'Watch Expressions'.");
      is(exprScope._store.size, 2,
        "There should be 2 evaluations available.");

      let w1 = exprScope.get("myVar.prop");
      let w2 = exprScope.get("myVar.prop + 42");
      let w3 = exprScope.get("myVar.prop = 'xlerb'");

      ok(w1, "The first watch expression should be present in the scope.");
      ok(w2, "The second watch expression should be present in the scope.");
      ok(!w3, "The third watch expression should not be present in the scope.");

      is(w1.value, "42", "The first expression value is correct.");
      is(w2.value, "84", "The second expression value is correct.");
    })
    .then(() => {
      let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS);
      gWatch.addExpression("myVar.prop = 'xlerb'");
      gEditor.focus();
      return finished;
    })
    .then(() => {
      let exprScope = gVars.getScopeAtIndex(0);
      ok(exprScope,
        "There should be a wach expressions scope in the variables view.");
      is(exprScope.name, gL10N.getStr("watchExpressionsScopeLabel"),
        "The scope's name should be marked as 'Watch Expressions'.");
      is(exprScope._store.size, 3,
        "There should be 3 evaluations available.");

      let w1 = exprScope.get("myVar.prop");
      let w2 = exprScope.get("myVar.prop + 42");
      let w3 = exprScope.get("myVar.prop = 'xlerb'");

      ok(w1, "The first watch expression should be present in the scope.");
      ok(w2, "The second watch expression should be present in the scope.");
      ok(w3, "The third watch expression should be present in the scope.");

      is(w1.value, "xlerb", "The first expression value is correct.");
      is(w2.value, "xlerb42", "The second expression value is correct.");
      is(w3.value, "xlerb", "The third expression value is correct.");
    });
}

function deleteWatchExpression(aString) {
  let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS);
  gWatch.deleteExpression({ name: aString });
  return finished;
}

function deleteLastWatchExpression(aString) {
  let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);
  gWatch.deleteExpression({ name: aString });
  return finished;
}

function testEdit(aWhat, aString, aExpected) {
  let localScope = gVars.getScopeAtIndex(1);
  let myVar = localScope.get("myVar");

  let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES).then(() => {
    let propVar = myVar.get("prop");
    let getterOrSetterOrVar = aWhat != "self" ? propVar.get(aWhat) : propVar;

    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_WATCH_EXPRESSIONS).then(() => {
      let exprScope = gVars.getScopeAtIndex(0);
      ok(exprScope,
        "There should be a wach expressions scope in the variables view.");
      is(exprScope.name, gL10N.getStr("watchExpressionsScopeLabel"),
        "The scope's name should be marked as 'Watch Expressions'.");
      is(exprScope._store.size, Object.keys(aExpected).length,
        "There should be a certain number of evaluations available.");

      function testExpression(aExpression) {
        if (!aExpression) {
          return;
        }
        let value = aExpected[aExpression.name];
        if (isNaN(value)) {
          ok(isNaN(aExpression.value),
            "The expression value is correct after the edit.");
        } else if (value == null) {
          is(aExpression.value.type, value + "",
            "The expression value is correct after the edit.");
        } else {
          is(aExpression.value, value,
            "The expression value is correct after the edit.");
        }
      }

      testExpression(exprScope.get(Object.keys(aExpected)[0]));
      testExpression(exprScope.get(Object.keys(aExpected)[1]));
      testExpression(exprScope.get(Object.keys(aExpected)[2]));
    });

    let editTarget = getterOrSetterOrVar.target;

    // Allow the target variable to get painted, so that clicking on
    // its value would scroll the new textbox node into view.
    executeSoon(() => {
      let varValue = editTarget.querySelector(".title > .value");
      EventUtils.sendMouseEvent({ type: "mousedown" }, varValue, gDebugger);

      let varInput = editTarget.querySelector(".title > .element-value-input");
      setText(varInput, aString);
      EventUtils.sendKey("RETURN", gDebugger);
    });

    return finished;
  });

  myVar.expand();
  gVars.clearHierarchy();

  return finished;
}

function testWatchExpressionsRemoved() {
  let scope = gVars.getScopeAtIndex(0);
  ok(scope,
    "There should be a local scope in the variables view.");
  isnot(scope.name, gL10N.getStr("watchExpressionsScopeLabel"),
    "The scope's name should not be marked as 'Watch Expressions'.");
  isnot(scope._store.size, 0,
    "There should be some variables available.");
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gL10N = null;
  gEditor = null;
  gVars = null;
  gWatch = null;
});
