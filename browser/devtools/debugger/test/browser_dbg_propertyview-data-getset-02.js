/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view is able to override getter properties
 * to plain value properties.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_frame-parameters.html";

var gPane = null;
var gTab = null;
var gDebugger = null;
var gVars = null;
var gWatch = null;

requestLongerTimeout(2);

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gVars = gDebugger.DebuggerView.Variables;
    gWatch = gDebugger.DebuggerView.WatchExpressions;

    gVars.switch = function() {};
    gVars.delete = function() {};

    prepareVariablesView();
  });
}

function prepareVariablesView() {
  gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      testVariablesView();

    }}, 0);
  }, false);

  EventUtils.sendMouseEvent({ type: "click" },
    content.document.querySelector("button"),
    content.window);
}

function testVariablesView()
{
  executeSoon(function() {
    addWatchExpressions(function() {
      testEdit("\"xlerb\"", "xlerb", function() {
        closeDebuggerAndFinish();
      });
    });
  });
}

function addWatchExpressions(callback)
{
  waitForWatchExpressions(function() {
    let label = gDebugger.L10N.getStr("watchExpressionsScopeLabel");
    let scope = gVars._currHierarchy.get(label);

    ok(scope, "There should be a wach expressions scope in the variables view");
    is(scope._store.size, 1, "There should be 1 evaluation availalble");

    let expr = scope.get("myVar.prop");
    ok(expr, "The watch expression should be present in the scope");
    is(expr.value, 42, "The value is correct.");

    callback();
  });

  gWatch.addExpression("myVar.prop");
  gDebugger.editor.focus();
}

function testEdit(string, expected, callback)
{
  let localScope = gDebugger.DebuggerView.Variables._list.querySelectorAll(".variables-view-scope")[1],
      localNodes = localScope.querySelector(".variables-view-element-details").childNodes,
      myVar = gVars.getItemForNode(localNodes[11]);

  waitForProperties(function() {
    let prop = myVar.get("prop");

    is(prop.ownerView.name, "myVar",
      "The right owner property name wasn't found.");
    is(prop.name, "prop",
      "The right property name wasn't found.");

    is(prop.ownerView.value.type, "object",
      "The right owner property value type wasn't found.");
    is(prop.ownerView.value.class, "Object",
      "The right owner property value class wasn't found.");

    is(prop.name, "prop",
      "The right property name wasn't found.");
    is(prop.value, undefined,
      "The right property value wasn't found.");
    ok(prop.getter,
      "The right property getter wasn't found.");
    ok(prop.setter,
      "The right property setter wasn't found.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      prop._target.querySelector(".variables-view-edit"),
      gDebugger);

    waitForElement(".element-value-input", true, function() {
      waitForWatchExpressions(function() {
        let label = gDebugger.L10N.getStr("watchExpressionsScopeLabel");
        let scope = gVars._currHierarchy.get(label);

        let expr = scope.get("myVar.prop");
        is(expr.value, expected, "The value is correct.");

        callback();
      });

      write(string);
      EventUtils.sendKey("RETURN", gDebugger);
    });
  });

  myVar.expand();
  gVars.clearHierarchy();
}

function waitForWatchExpressions(callback) {
  gDebugger.addEventListener("Debugger:FetchedWatchExpressions", function onFetch() {
    gDebugger.removeEventListener("Debugger:FetchedWatchExpressions", onFetch, false);
    executeSoon(callback);
  }, false);
}

function waitForProperties(callback) {
  gDebugger.addEventListener("Debugger:FetchedProperties", function onFetch() {
    gDebugger.removeEventListener("Debugger:FetchedProperties", onFetch, false);
    executeSoon(callback);
  }, false);
}

function waitForElement(selector, exists, callback)
{
  // Poll every few milliseconds until the element are retrieved.
  let count = 0;
  let intervalID = window.setInterval(function() {
    info("count: " + count + " ");
    if (++count > 50) {
      ok(false, "Timed out while polling for the element.");
      window.clearInterval(intervalID);
      return closeDebuggerAndFinish();
    }
    if (!!gVars._list.querySelector(selector) != exists) {
      return;
    }
    // We got the element, it's safe to callback.
    window.clearInterval(intervalID);
    callback();
  }, 100);
}

function write(text) {
  if (!text) {
    EventUtils.sendKey("BACK_SPACE", gDebugger);
    return;
  }
  for (let i = 0; i < text.length; i++) {
    EventUtils.sendChar(text[i], gDebugger);
  }
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
  gVars = null;
  gWatch = null;
});
