/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view is correctly populated in 'with' frames.
 */

const TAB_URL = EXAMPLE_URL + "doc_with-frame.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gVariables;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gVariables = gDebugger.DebuggerView.Variables;

    // The first 'with' scope should be expanded by default, but the
    // variables haven't been fetched yet. This is how 'with' scopes work.
    promise.all([
      waitForSourceAndCaret(gPanel, ".html", 22),
      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES),
      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_VARIABLES)
    ]).then(testFirstWithScope)
      .then(expandSecondWithScope)
      .then(testSecondWithScope)
      .then(expandFunctionScope)
      .then(testFunctionScope)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    EventUtils.sendMouseEvent({ type: "click" },
      gDebuggee.document.querySelector("button"),
      gDebuggee);
  });
}

function testFirstWithScope() {
  let firstWithScope = gVariables.getScopeAtIndex(0);
  is(firstWithScope.expanded, true,
    "The first 'with' scope should be expanded by default.");
  ok(firstWithScope.target.querySelector(".name").getAttribute("value").contains("[Object]"),
    "The first 'with' scope should be properly identified.");

  let withEnums = firstWithScope._enum.childNodes;
  let withNonEnums = firstWithScope._nonenum.childNodes;

  is(withEnums.length, 3,
    "The first 'with' scope should contain all the created enumerable elements.");
  is(withNonEnums.length, 1,
    "The first 'with' scope should contain all the created non-enumerable elements.");

  is(withEnums[0].querySelector(".name").getAttribute("value"), "this",
    "Should have the right property name for 'this'.");
  is(withEnums[0].querySelector(".value").getAttribute("value"),
    "Window \u2192 doc_with-frame.html",
    "Should have the right property value for 'this'.");
  ok(withEnums[0].querySelector(".value").className.contains("token-other"),
    "Should have the right token class for 'this'.");

  is(withEnums[1].querySelector(".name").getAttribute("value"), "alpha",
    "Should have the right property name for 'alpha'.");
  is(withEnums[1].querySelector(".value").getAttribute("value"), "1",
    "Should have the right property value for 'alpha'.");
  ok(withEnums[1].querySelector(".value").className.contains("token-number"),
    "Should have the right token class for 'alpha'.");

  is(withEnums[2].querySelector(".name").getAttribute("value"), "beta",
    "Should have the right property name for 'beta'.");
  is(withEnums[2].querySelector(".value").getAttribute("value"), "2",
    "Should have the right property value for 'beta'.");
  ok(withEnums[2].querySelector(".value").className.contains("token-number"),
    "Should have the right token class for 'beta'.");

  is(withNonEnums[0].querySelector(".name").getAttribute("value"), "__proto__",
   "Should have the right property name for '__proto__'.");
  is(withNonEnums[0].querySelector(".value").getAttribute("value"), "Object",
   "Should have the right property value for '__proto__'.");
  ok(withNonEnums[0].querySelector(".value").className.contains("token-other"),
   "Should have the right token class for '__proto__'.");
}

function expandSecondWithScope() {
  let deferred = promise.defer();

  let secondWithScope = gVariables.getScopeAtIndex(1);
  is(secondWithScope.expanded, false,
    "The second 'with' scope should not be expanded by default.");

  gDebugger.once(gDebugger.EVENTS.FETCHED_VARIABLES, deferred.resolve);

  EventUtils.sendMouseEvent({ type: "mousedown" },
    secondWithScope.target.querySelector(".name"),
    gDebugger);

  return deferred.promise;
}

function testSecondWithScope() {
  let secondWithScope = gVariables.getScopeAtIndex(1);
  is(secondWithScope.expanded, true,
    "The second 'with' scope should now be expanded.");
  ok(secondWithScope.target.querySelector(".name").getAttribute("value").contains("[Math]"),
    "The second 'with' scope should be properly identified.");

  let withEnums = secondWithScope._enum.childNodes;
  let withNonEnums = secondWithScope._nonenum.childNodes;

  is(withEnums.length, 0,
    "The second 'with' scope should contain all the created enumerable elements.");
  isnot(withNonEnums.length, 0,
    "The second 'with' scope should contain all the created non-enumerable elements.");

  is(secondWithScope.get("E").target.querySelector(".name").getAttribute("value"), "E",
    "Should have the right property name for 'E'.");
  is(secondWithScope.get("E").target.querySelector(".value").getAttribute("value"), "2.718281828459045",
    "Should have the right property value for 'E'.");
  ok(secondWithScope.get("E").target.querySelector(".value").className.contains("token-number"),
    "Should have the right token class for 'E'.");

  is(secondWithScope.get("PI").target.querySelector(".name").getAttribute("value"), "PI",
    "Should have the right property name for 'PI'.");
  is(secondWithScope.get("PI").target.querySelector(".value").getAttribute("value"), "3.141592653589793",
    "Should have the right property value for 'PI'.");
  ok(secondWithScope.get("PI").target.querySelector(".value").className.contains("token-number"),
    "Should have the right token class for 'PI'.");

  is(secondWithScope.get("random").target.querySelector(".name").getAttribute("value"), "random",
    "Should have the right property name for 'random'.");
  is(secondWithScope.get("random").target.querySelector(".value").getAttribute("value"), "random()",
    "Should have the right property value for 'random'.");
  ok(secondWithScope.get("random").target.querySelector(".value").className.contains("token-other"),
    "Should have the right token class for 'random'.");

  is(secondWithScope.get("__proto__").target.querySelector(".name").getAttribute("value"), "__proto__",
    "Should have the right property name for '__proto__'.");
  is(secondWithScope.get("__proto__").target.querySelector(".value").getAttribute("value"), "Object",
    "Should have the right property value for '__proto__'.");
  ok(secondWithScope.get("__proto__").target.querySelector(".value").className.contains("token-other"),
    "Should have the right token class for '__proto__'.");
}

function expandFunctionScope() {
  let funcScope = gVariables.getScopeAtIndex(2);
  is(funcScope.expanded, false,
    "The function scope shouldn't be expanded by default, but the " +
    "variables have been already fetched. This is how local scopes work.");

  EventUtils.sendMouseEvent({ type: "mousedown" },
    funcScope.target.querySelector(".name"),
    gDebugger);

  return promise.resolve(null);
}

function testFunctionScope() {
  let funcScope = gVariables.getScopeAtIndex(2);
  is(funcScope.expanded, true,
    "The function scope should now be expanded.");
  ok(funcScope.target.querySelector(".name").getAttribute("value").contains("[test]"),
    "The function scope should be properly identified.");

  let funcEnums = funcScope._enum.childNodes;
  let funcNonEnums = funcScope._nonenum.childNodes;

  is(funcEnums.length, 6,
    "The function scope should contain all the created enumerable elements.");
  is(funcNonEnums.length, 0,
    "The function scope should contain all the created non-enumerable elements.");

  is(funcScope.get("aNumber").target.querySelector(".name").getAttribute("value"), "aNumber",
    "Should have the right property name for 'aNumber'.");
  is(funcScope.get("aNumber").target.querySelector(".value").getAttribute("value"), "10",
    "Should have the right property value for 'aNumber'.");
  ok(funcScope.get("aNumber").target.querySelector(".value").className.contains("token-number"),
    "Should have the right token class for 'aNumber'.");

  is(funcScope.get("a").target.querySelector(".name").getAttribute("value"), "a",
    "Should have the right property name for 'a'.");
  is(funcScope.get("a").target.querySelector(".value").getAttribute("value"), "314.1592653589793",
    "Should have the right property value for 'a'.");
  ok(funcScope.get("a").target.querySelector(".value").className.contains("token-number"),
    "Should have the right token class for 'a'.");

  is(funcScope.get("r").target.querySelector(".name").getAttribute("value"), "r",
    "Should have the right property name for 'r'.");
  is(funcScope.get("r").target.querySelector(".value").getAttribute("value"), "10",
    "Should have the right property value for 'r'.");
  ok(funcScope.get("r").target.querySelector(".value").className.contains("token-number"),
    "Should have the right token class for 'r'.");

  is(funcScope.get("foo").target.querySelector(".name").getAttribute("value"), "foo",
    "Should have the right property name for 'foo'.");
  is(funcScope.get("foo").target.querySelector(".value").getAttribute("value"), "6.283185307179586",
    "Should have the right property value for 'foo'.");
  ok(funcScope.get("foo").target.querySelector(".value").className.contains("token-number"),
    "Should have the right token class for 'foo'.");
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gVariables = null;
});
