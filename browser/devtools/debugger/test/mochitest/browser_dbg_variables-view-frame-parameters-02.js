/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view displays the right variables and
 * properties when debugger is paused.
 */

const TAB_URL = EXAMPLE_URL + "doc_frame-parameters.html";

var gTab, gPanel, gDebugger;
var gVariables;

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gVariables = gDebugger.DebuggerView.Variables;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 24)
      .then(testScopeVariables)
      .then(testArgumentsProperties)
      .then(testSimpleObject)
      .then(testComplexObject)
      .then(testArgumentObject)
      .then(testInnerArgumentObject)
      .then(testGetterSetterObject)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    generateMouseClickInTab(gTab, "content.document.querySelector('button')");
  });
}

function testScopeVariables() {
  let localScope = gVariables.getScopeAtIndex(0);
  is(localScope.expanded, true,
    "The local scope should be expanded by default.");

  let localEnums = localScope.target.querySelector(".variables-view-element-details.enum").childNodes;
  let localNonEnums = localScope.target.querySelector(".variables-view-element-details.nonenum").childNodes;

  is(localEnums.length, 12,
    "The local scope should contain all the created enumerable elements.");
  is(localNonEnums.length, 0,
    "The local scope should contain all the created non-enumerable elements.");

  is(localEnums[0].querySelector(".name").getAttribute("value"), "this",
    "Should have the right property name for 'this'.");
  is(localEnums[0].querySelector(".value").getAttribute("value"),
    "Window \u2192 doc_frame-parameters.html",
    "Should have the right property value for 'this'.");
  ok(localEnums[0].querySelector(".value").className.includes("token-other"),
    "Should have the right token class for 'this'.");

  is(localEnums[1].querySelector(".name").getAttribute("value"), "aArg",
    "Should have the right property name for 'aArg'.");
  is(localEnums[1].querySelector(".value").getAttribute("value"), "Object",
    "Should have the right property value for 'aArg'.");
  ok(localEnums[1].querySelector(".value").className.includes("token-other"),
    "Should have the right token class for 'aArg'.");

  is(localEnums[2].querySelector(".name").getAttribute("value"), "bArg",
    "Should have the right property name for 'bArg'.");
  is(localEnums[2].querySelector(".value").getAttribute("value"), "\"beta\"",
    "Should have the right property value for 'bArg'.");
  ok(localEnums[2].querySelector(".value").className.includes("token-string"),
    "Should have the right token class for 'bArg'.");

  is(localEnums[3].querySelector(".name").getAttribute("value"), "cArg",
    "Should have the right property name for 'cArg'.");
  is(localEnums[3].querySelector(".value").getAttribute("value"), "3",
    "Should have the right property value for 'cArg'.");
  ok(localEnums[3].querySelector(".value").className.includes("token-number"),
    "Should have the right token class for 'cArg'.");

  is(localEnums[4].querySelector(".name").getAttribute("value"), "dArg",
    "Should have the right property name for 'dArg'.");
  is(localEnums[4].querySelector(".value").getAttribute("value"), "false",
    "Should have the right property value for 'dArg'.");
  ok(localEnums[4].querySelector(".value").className.includes("token-boolean"),
    "Should have the right token class for 'dArg'.");

  is(localEnums[5].querySelector(".name").getAttribute("value"), "eArg",
    "Should have the right property name for 'eArg'.");
  is(localEnums[5].querySelector(".value").getAttribute("value"), "null",
    "Should have the right property value for 'eArg'.");
  ok(localEnums[5].querySelector(".value").className.includes("token-null"),
    "Should have the right token class for 'eArg'.");

  is(localEnums[6].querySelector(".name").getAttribute("value"), "fArg",
    "Should have the right property name for 'fArg'.");
  is(localEnums[6].querySelector(".value").getAttribute("value"), "undefined",
    "Should have the right property value for 'fArg'.");
  ok(localEnums[6].querySelector(".value").className.includes("token-undefined"),
    "Should have the right token class for 'fArg'.");

  is(localEnums[7].querySelector(".name").getAttribute("value"), "a",
   "Should have the right property name for 'a'.");
  is(localEnums[7].querySelector(".value").getAttribute("value"), "1",
   "Should have the right property value for 'a'.");
  ok(localEnums[7].querySelector(".value").className.includes("token-number"),
   "Should have the right token class for 'a'.");

  is(localEnums[8].querySelector(".name").getAttribute("value"), "arguments",
    "Should have the right property name for 'arguments'.");
  is(localEnums[8].querySelector(".value").getAttribute("value"), "Arguments",
    "Should have the right property value for 'arguments'.");
  ok(localEnums[8].querySelector(".value").className.includes("token-other"),
    "Should have the right token class for 'arguments'.");

  is(localEnums[9].querySelector(".name").getAttribute("value"), "b",
   "Should have the right property name for 'b'.");
  is(localEnums[9].querySelector(".value").getAttribute("value"), "Object",
   "Should have the right property value for 'b'.");
  ok(localEnums[9].querySelector(".value").className.includes("token-other"),
   "Should have the right token class for 'b'.");

  is(localEnums[10].querySelector(".name").getAttribute("value"), "c",
   "Should have the right property name for 'c'.");
  is(localEnums[10].querySelector(".value").getAttribute("value"), "Object",
   "Should have the right property value for 'c'.");
  ok(localEnums[10].querySelector(".value").className.includes("token-other"),
   "Should have the right token class for 'c'.");

  is(localEnums[11].querySelector(".name").getAttribute("value"), "myVar",
   "Should have the right property name for 'myVar'.");
  is(localEnums[11].querySelector(".value").getAttribute("value"), "Object",
   "Should have the right property value for 'myVar'.");
  ok(localEnums[11].querySelector(".value").className.includes("token-other"),
   "Should have the right token class for 'myVar'.");
}

function testArgumentsProperties() {
  let deferred = promise.defer();

  let argsVar = gVariables.getScopeAtIndex(0).get("arguments");
  is(argsVar.expanded, false,
    "The 'arguments' variable should not be expanded by default.");

  let argsEnums = argsVar.target.querySelector(".variables-view-element-details.enum").childNodes;
  let argsNonEnums = argsVar.target.querySelector(".variables-view-element-details.nonenum").childNodes;

  gDebugger.once(gDebugger.EVENTS.FETCHED_PROPERTIES, () => {
    is(argsEnums.length, 5,
      "The 'arguments' variable should contain all the created enumerable elements.");
    is(argsNonEnums.length, 3,
      "The 'arguments' variable should contain all the created non-enumerable elements.");

    is(argsEnums[0].querySelector(".name").getAttribute("value"), "0",
      "Should have the right property name for '0'.");
    is(argsEnums[0].querySelector(".value").getAttribute("value"), "Object",
      "Should have the right property value for '0'.");
    ok(argsEnums[0].querySelector(".value").className.includes("token-other"),
      "Should have the right token class for '0'.");

    is(argsEnums[1].querySelector(".name").getAttribute("value"), "1",
      "Should have the right property name for '1'.");
    is(argsEnums[1].querySelector(".value").getAttribute("value"), "\"beta\"",
      "Should have the right property value for '1'.");
    ok(argsEnums[1].querySelector(".value").className.includes("token-string"),
      "Should have the right token class for '1'.");

    is(argsEnums[2].querySelector(".name").getAttribute("value"), "2",
      "Should have the right property name for '2'.");
    is(argsEnums[2].querySelector(".value").getAttribute("value"), "3",
      "Should have the right property name for '2'.");
    ok(argsEnums[2].querySelector(".value").className.includes("token-number"),
      "Should have the right token class for '2'.");

    is(argsEnums[3].querySelector(".name").getAttribute("value"), "3",
      "Should have the right property name for '3'.");
    is(argsEnums[3].querySelector(".value").getAttribute("value"), "false",
      "Should have the right property value for '3'.");
    ok(argsEnums[3].querySelector(".value").className.includes("token-boolean"),
      "Should have the right token class for '3'.");

    is(argsEnums[4].querySelector(".name").getAttribute("value"), "4",
      "Should have the right property name for '4'.");
    is(argsEnums[4].querySelector(".value").getAttribute("value"), "null",
      "Should have the right property name for '4'.");
    ok(argsEnums[4].querySelector(".value").className.includes("token-null"),
      "Should have the right token class for '4'.");

    is(argsNonEnums[0].querySelector(".name").getAttribute("value"), "callee",
     "Should have the right property name for 'callee'.");
    is(argsNonEnums[0].querySelector(".value").getAttribute("value"),
     "test(aArg,bArg,cArg,dArg,eArg,fArg)",
     "Should have the right property name for 'callee'.");
    ok(argsNonEnums[0].querySelector(".value").className.includes("token-other"),
     "Should have the right token class for 'callee'.");

    is(argsNonEnums[1].querySelector(".name").getAttribute("value"), "length",
      "Should have the right property name for 'length'.");
    is(argsNonEnums[1].querySelector(".value").getAttribute("value"), "5",
      "Should have the right property value for 'length'.");
    ok(argsNonEnums[1].querySelector(".value").className.includes("token-number"),
      "Should have the right token class for 'length'.");

    is(argsNonEnums[2].querySelector(".name").getAttribute("value"), "__proto__",
     "Should have the right property name for '__proto__'.");
    is(argsNonEnums[2].querySelector(".value").getAttribute("value"), "Object",
     "Should have the right property value for '__proto__'.");
    ok(argsNonEnums[2].querySelector(".value").className.includes("token-other"),
     "Should have the right token class for '__proto__'.");

    deferred.resolve();
  });

  argsVar.expand();
  return deferred.promise;
}

function testSimpleObject() {
  let deferred = promise.defer();

  let bVar = gVariables.getScopeAtIndex(0).get("b");
  is(bVar.expanded, false,
    "The 'b' variable should not be expanded by default.");

  let bEnums = bVar.target.querySelector(".variables-view-element-details.enum").childNodes;
  let bNonEnums = bVar.target.querySelector(".variables-view-element-details.nonenum").childNodes;

  gDebugger.once(gDebugger.EVENTS.FETCHED_PROPERTIES, () => {
    is(bEnums.length, 1,
      "The 'b' variable should contain all the created enumerable elements.");
    is(bNonEnums.length, 1,
      "The 'b' variable should contain all the created non-enumerable elements.");

    is(bEnums[0].querySelector(".name").getAttribute("value"), "a",
      "Should have the right property name for 'a'.");
    is(bEnums[0].querySelector(".value").getAttribute("value"), "1",
      "Should have the right property value for 'a'.");
    ok(bEnums[0].querySelector(".value").className.includes("token-number"),
      "Should have the right token class for 'a'.");

    is(bNonEnums[0].querySelector(".name").getAttribute("value"), "__proto__",
     "Should have the right property name for '__proto__'.");
    is(bNonEnums[0].querySelector(".value").getAttribute("value"), "Object",
     "Should have the right property value for '__proto__'.");
    ok(bNonEnums[0].querySelector(".value").className.includes("token-other"),
     "Should have the right token class for '__proto__'.");

    deferred.resolve();
  });

  bVar.expand();
  return deferred.promise;
}

function testComplexObject() {
  let deferred = promise.defer();

  let cVar = gVariables.getScopeAtIndex(0).get("c");
  is(cVar.expanded, false,
    "The 'c' variable should not be expanded by default.");

  let cEnums = cVar.target.querySelector(".variables-view-element-details.enum").childNodes;
  let cNonEnums = cVar.target.querySelector(".variables-view-element-details.nonenum").childNodes;

  gDebugger.once(gDebugger.EVENTS.FETCHED_PROPERTIES, () => {
    is(cEnums.length, 6,
      "The 'c' variable should contain all the created enumerable elements.");
    is(cNonEnums.length, 1,
      "The 'c' variable should contain all the created non-enumerable elements.");

    is(cEnums[0].querySelector(".name").getAttribute("value"), "a",
      "Should have the right property name for 'a'.");
    is(cEnums[0].querySelector(".value").getAttribute("value"), "1",
      "Should have the right property value for 'a'.");
    ok(cEnums[0].querySelector(".value").className.includes("token-number"),
      "Should have the right token class for 'a'.");

    is(cEnums[1].querySelector(".name").getAttribute("value"), "b",
      "Should have the right property name for 'b'.");
    is(cEnums[1].querySelector(".value").getAttribute("value"), "\"beta\"",
      "Should have the right property value for 'b'.");
    ok(cEnums[1].querySelector(".value").className.includes("token-string"),
      "Should have the right token class for 'b'.");

    is(cEnums[2].querySelector(".name").getAttribute("value"), "c",
      "Should have the right property name for 'c'.");
    is(cEnums[2].querySelector(".value").getAttribute("value"), "3",
      "Should have the right property value for 'c'.");
    ok(cEnums[2].querySelector(".value").className.includes("token-number"),
      "Should have the right token class for 'c'.");

    is(cEnums[3].querySelector(".name").getAttribute("value"), "d",
      "Should have the right property name for 'd'.");
    is(cEnums[3].querySelector(".value").getAttribute("value"), "false",
      "Should have the right property value for 'd'.");
    ok(cEnums[3].querySelector(".value").className.includes("token-boolean"),
      "Should have the right token class for 'd'.");

    is(cEnums[4].querySelector(".name").getAttribute("value"), "e",
      "Should have the right property name for 'e'.");
    is(cEnums[4].querySelector(".value").getAttribute("value"), "null",
      "Should have the right property value for 'e'.");
    ok(cEnums[4].querySelector(".value").className.includes("token-null"),
      "Should have the right token class for 'e'.");

    is(cEnums[5].querySelector(".name").getAttribute("value"), "f",
      "Should have the right property name for 'f'.");
    is(cEnums[5].querySelector(".value").getAttribute("value"), "undefined",
      "Should have the right property value for 'f'.");
    ok(cEnums[5].querySelector(".value").className.includes("token-undefined"),
      "Should have the right token class for 'f'.");

    is(cNonEnums[0].querySelector(".name").getAttribute("value"), "__proto__",
     "Should have the right property name for '__proto__'.");
    is(cNonEnums[0].querySelector(".value").getAttribute("value"), "Object",
     "Should have the right property value for '__proto__'.");
    ok(cNonEnums[0].querySelector(".value").className.includes("token-other"),
     "Should have the right token class for '__proto__'.");

    deferred.resolve();
  });

  cVar.expand();
  return deferred.promise;
}

function testArgumentObject() {
  let deferred = promise.defer();

  let argVar = gVariables.getScopeAtIndex(0).get("aArg");
  is(argVar.expanded, false,
    "The 'aArg' variable should not be expanded by default.");

  let argEnums = argVar.target.querySelector(".variables-view-element-details.enum").childNodes;
  let argNonEnums = argVar.target.querySelector(".variables-view-element-details.nonenum").childNodes;

  gDebugger.once(gDebugger.EVENTS.FETCHED_PROPERTIES, () => {
    is(argEnums.length, 6,
      "The 'aArg' variable should contain all the created enumerable elements.");
    is(argNonEnums.length, 1,
      "The 'aArg' variable should contain all the created non-enumerable elements.");

    is(argEnums[0].querySelector(".name").getAttribute("value"), "a",
      "Should have the right property name for 'a'.");
    is(argEnums[0].querySelector(".value").getAttribute("value"), "1",
      "Should have the right property value for 'a'.");
    ok(argEnums[0].querySelector(".value").className.includes("token-number"),
      "Should have the right token class for 'a'.");

    is(argEnums[1].querySelector(".name").getAttribute("value"), "b",
      "Should have the right property name for 'b'.");
    is(argEnums[1].querySelector(".value").getAttribute("value"), "\"beta\"",
      "Should have the right property value for 'b'.");
    ok(argEnums[1].querySelector(".value").className.includes("token-string"),
      "Should have the right token class for 'b'.");

    is(argEnums[2].querySelector(".name").getAttribute("value"), "c",
      "Should have the right property name for 'c'.");
    is(argEnums[2].querySelector(".value").getAttribute("value"), "3",
      "Should have the right property value for 'c'.");
    ok(argEnums[2].querySelector(".value").className.includes("token-number"),
      "Should have the right token class for 'c'.");

    is(argEnums[3].querySelector(".name").getAttribute("value"), "d",
      "Should have the right property name for 'd'.");
    is(argEnums[3].querySelector(".value").getAttribute("value"), "false",
      "Should have the right property value for 'd'.");
    ok(argEnums[3].querySelector(".value").className.includes("token-boolean"),
      "Should have the right token class for 'd'.");

    is(argEnums[4].querySelector(".name").getAttribute("value"), "e",
      "Should have the right property name for 'e'.");
    is(argEnums[4].querySelector(".value").getAttribute("value"), "null",
      "Should have the right property value for 'e'.");
    ok(argEnums[4].querySelector(".value").className.includes("token-null"),
      "Should have the right token class for 'e'.");

    is(argEnums[5].querySelector(".name").getAttribute("value"), "f",
      "Should have the right property name for 'f'.");
    is(argEnums[5].querySelector(".value").getAttribute("value"), "undefined",
      "Should have the right property value for 'f'.");
    ok(argEnums[5].querySelector(".value").className.includes("token-undefined"),
      "Should have the right token class for 'f'.");

    is(argNonEnums[0].querySelector(".name").getAttribute("value"), "__proto__",
     "Should have the right property name for '__proto__'.");
    is(argNonEnums[0].querySelector(".value").getAttribute("value"), "Object",
     "Should have the right property value for '__proto__'.");
    ok(argNonEnums[0].querySelector(".value").className.includes("token-other"),
     "Should have the right token class for '__proto__'.");

    deferred.resolve();
  });

  argVar.expand();
  return deferred.promise;
}

function testInnerArgumentObject() {
  let deferred = promise.defer();

  let argProp = gVariables.getScopeAtIndex(0).get("arguments").get("0");
  is(argProp.expanded, false,
    "The 'arguments[0]' property should not be expanded by default.");

  let argEnums = argProp.target.querySelector(".variables-view-element-details.enum").childNodes;
  let argNonEnums = argProp.target.querySelector(".variables-view-element-details.nonenum").childNodes;

  gDebugger.once(gDebugger.EVENTS.FETCHED_PROPERTIES, () => {
    is(argEnums.length, 6,
      "The 'arguments[0]' property should contain all the created enumerable elements.");
    is(argNonEnums.length, 1,
      "The 'arguments[0]' property should contain all the created non-enumerable elements.");

    is(argEnums[0].querySelector(".name").getAttribute("value"), "a",
      "Should have the right property name for 'a'.");
    is(argEnums[0].querySelector(".value").getAttribute("value"), "1",
      "Should have the right property value for 'a'.");
    ok(argEnums[0].querySelector(".value").className.includes("token-number"),
      "Should have the right token class for 'a'.");

    is(argEnums[1].querySelector(".name").getAttribute("value"), "b",
      "Should have the right property name for 'b'.");
    is(argEnums[1].querySelector(".value").getAttribute("value"), "\"beta\"",
      "Should have the right property value for 'b'.");
    ok(argEnums[1].querySelector(".value").className.includes("token-string"),
      "Should have the right token class for 'b'.");

    is(argEnums[2].querySelector(".name").getAttribute("value"), "c",
      "Should have the right property name for 'c'.");
    is(argEnums[2].querySelector(".value").getAttribute("value"), "3",
      "Should have the right property value for 'c'.");
    ok(argEnums[2].querySelector(".value").className.includes("token-number"),
      "Should have the right token class for 'c'.");

    is(argEnums[3].querySelector(".name").getAttribute("value"), "d",
      "Should have the right property name for 'd'.");
    is(argEnums[3].querySelector(".value").getAttribute("value"), "false",
      "Should have the right property value for 'd'.");
    ok(argEnums[3].querySelector(".value").className.includes("token-boolean"),
      "Should have the right token class for 'd'.");

    is(argEnums[4].querySelector(".name").getAttribute("value"), "e",
      "Should have the right property name for 'e'.");
    is(argEnums[4].querySelector(".value").getAttribute("value"), "null",
      "Should have the right property value for 'e'.");
    ok(argEnums[4].querySelector(".value").className.includes("token-null"),
      "Should have the right token class for 'e'.");

    is(argEnums[5].querySelector(".name").getAttribute("value"), "f",
      "Should have the right property name for 'f'.");
    is(argEnums[5].querySelector(".value").getAttribute("value"), "undefined",
      "Should have the right property value for 'f'.");
    ok(argEnums[5].querySelector(".value").className.includes("token-undefined"),
      "Should have the right token class for 'f'.");

    is(argNonEnums[0].querySelector(".name").getAttribute("value"), "__proto__",
     "Should have the right property name for '__proto__'.");
    is(argNonEnums[0].querySelector(".value").getAttribute("value"), "Object",
     "Should have the right property value for '__proto__'.");
    ok(argNonEnums[0].querySelector(".value").className.includes("token-other"),
     "Should have the right token class for '__proto__'.");

    deferred.resolve();
  });

  argProp.expand();
  return deferred.promise;
}

function testGetterSetterObject() {
  let deferred = promise.defer();

  let myVar = gVariables.getScopeAtIndex(0).get("myVar");
  is(myVar.expanded, false,
    "The myVar variable should not be expanded by default.");

  let myVarEnums = myVar.target.querySelector(".variables-view-element-details.enum").childNodes;
  let myVarNonEnums = myVar.target.querySelector(".variables-view-element-details.nonenum").childNodes;

  gDebugger.once(gDebugger.EVENTS.FETCHED_PROPERTIES, () => {
    is(myVarEnums.length, 2,
      "The myVar should contain all the created enumerable elements.");
    is(myVarNonEnums.length, 1,
      "The myVar should contain all the created non-enumerable elements.");

    is(myVarEnums[0].querySelector(".name").getAttribute("value"), "_prop",
      "Should have the right property name for '_prop'.");
    is(myVarEnums[0].querySelector(".value").getAttribute("value"), "42",
      "Should have the right property value for '_prop'.");
    ok(myVarEnums[0].querySelector(".value").className.includes("token-number"),
      "Should have the right token class for '_prop'.");

    is(myVarEnums[1].querySelector(".name").getAttribute("value"), "prop",
      "Should have the right property name for 'prop'.");
    is(myVarEnums[1].querySelector(".value").getAttribute("value"), "",
      "Should have the right property value for 'prop'.");
    ok(!myVarEnums[1].querySelector(".value").className.includes("token"),
      "Should have no token class for 'prop'.");

    is(myVarNonEnums[0].querySelector(".name").getAttribute("value"), "__proto__",
     "Should have the right property name for '__proto__'.");
    is(myVarNonEnums[0].querySelector(".value").getAttribute("value"), "Object",
     "Should have the right property value for '__proto__'.");
    ok(myVarNonEnums[0].querySelector(".value").className.includes("token-other"),
     "Should have the right token class for '__proto__'.");

    let propEnums = myVarEnums[1].querySelector(".variables-view-element-details.enum").childNodes;
    let propNonEnums = myVarEnums[1].querySelector(".variables-view-element-details.nonenum").childNodes;

    is(propEnums.length, 0,
      "The propEnums should contain all the created enumerable elements.");
    is(propNonEnums.length, 2,
      "The propEnums should contain all the created non-enumerable elements.");

    is(propNonEnums[0].querySelector(".name").getAttribute("value"), "get",
      "Should have the right property name for 'get'.");
    is(propNonEnums[0].querySelector(".value").getAttribute("value"),
      "test/myVar.prop()",
      "Should have the right property value for 'get'.");
    ok(propNonEnums[0].querySelector(".value").className.includes("token-other"),
      "Should have the right token class for 'get'.");

    is(propNonEnums[1].querySelector(".name").getAttribute("value"), "set",
      "Should have the right property name for 'set'.");
    is(propNonEnums[1].querySelector(".value").getAttribute("value"),
      "test/myVar.prop(val)",
      "Should have the right property value for 'set'.");
    ok(propNonEnums[1].querySelector(".value").className.includes("token-other"),
      "Should have the right token class for 'set'.");

    deferred.resolve();
  });

  myVar.expand();
  return deferred.promise;
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gVariables = null;
});
