/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view correctly displays the properties
 * of objects when debugger is paused.
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
      .then(initialChecks)
      .then(testExpandVariables)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .catch(aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    generateMouseClickInTab(gTab, "content.document.querySelector('button')");
  });
}

function initialChecks() {
  let scopeNodes = gDebugger.document.querySelectorAll(".variables-view-scope");
  is(scopeNodes.length, 3,
    "There should be 3 scopes available.");

  ok(scopeNodes[0].querySelector(".name").getAttribute("value").includes("[test]"),
    "The local scope should be properly identified.");
  ok(scopeNodes[1].querySelector(".name").getAttribute("value").includes("Block"),
    "The global lexical scope should be properly identified.");
  ok(scopeNodes[2].querySelector(".name").getAttribute("value").includes("[Window]"),
    "The global scope should be properly identified.");

  is(gVariables.getScopeAtIndex(0).target, scopeNodes[0],
    "getScopeAtIndex(0) didn't return the expected scope.");
  is(gVariables.getScopeAtIndex(1).target, scopeNodes[1],
    "getScopeAtIndex(1) didn't return the expected scope.");
  is(gVariables.getScopeAtIndex(2).target, scopeNodes[2],
    "getScopeAtIndex(2) didn't return the expected scope.");

  is(gVariables.getItemForNode(scopeNodes[0]).target, scopeNodes[0],
    "getItemForNode([0]) didn't return the expected scope.");
  is(gVariables.getItemForNode(scopeNodes[1]).target, scopeNodes[1],
    "getItemForNode([1]) didn't return the expected scope.");
  is(gVariables.getItemForNode(scopeNodes[2]).target, scopeNodes[2],
    "getItemForNode([2]) didn't return the expected scope.");

  is(gVariables.getItemForNode(scopeNodes[0]).expanded, true,
    "The local scope should be expanded by default.");
  is(gVariables.getItemForNode(scopeNodes[1]).expanded, false,
    "The global lexical scope should not be collapsed by default.");
  is(gVariables.getItemForNode(scopeNodes[2]).expanded, false,
    "The global scope should not be collapsed by default.");
}

function testExpandVariables() {
  let deferred = promise.defer();

  let localScope = gVariables.getScopeAtIndex(0);
  let localEnums = localScope.target.querySelector(".variables-view-element-details.enum").childNodes;

  let thisVar = gVariables.getItemForNode(localEnums[0]);
  let argsVar = gVariables.getItemForNode(localEnums[8]);
  let cVar = gVariables.getItemForNode(localEnums[10]);

  is(thisVar.target.querySelector(".name").getAttribute("value"), "this",
    "Should have the right property name for 'this'.");
  is(argsVar.target.querySelector(".name").getAttribute("value"), "arguments",
    "Should have the right property name for 'arguments'.");
  is(cVar.target.querySelector(".name").getAttribute("value"), "c",
    "Should have the right property name for 'c'.");

  is(thisVar.expanded, false,
    "The thisVar should not be expanded at this point.");
  is(argsVar.expanded, false,
    "The argsVar should not be expanded at this point.");
  is(cVar.expanded, false,
    "The cVar should not be expanded at this point.");

  waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_PROPERTIES, 3).then(() => {
    is(thisVar.get("window").target.querySelector(".name").getAttribute("value"), "window",
      "Should have the right property name for 'window'.");
    is(thisVar.get("window").target.querySelector(".value").getAttribute("value"),
      "Window \u2192 doc_frame-parameters.html",
      "Should have the right property value for 'window'.");
    ok(thisVar.get("window").target.querySelector(".value").className.includes("token-other"),
      "Should have the right token class for 'window'.");

    is(thisVar.get("document").target.querySelector(".name").getAttribute("value"), "document",
      "Should have the right property name for 'document'.");
    is(thisVar.get("document").target.querySelector(".value").getAttribute("value"),
      "HTMLDocument \u2192 doc_frame-parameters.html",
      "Should have the right property value for 'document'.");
    ok(thisVar.get("document").target.querySelector(".value").className.includes("token-domnode"),
      "Should have the right token class for 'document'.");

    let argsProps = argsVar.target.querySelectorAll(".variables-view-property");
    is(argsProps.length, 8,
      "The 'arguments' variable should contain 5 enumerable and 3 non-enumerable properties");

    is(argsProps[0].querySelector(".name").getAttribute("value"), "0",
      "Should have the right property name for '0'.");
    is(argsProps[0].querySelector(".value").getAttribute("value"), "Object",
      "Should have the right property value for '0'.");
    ok(argsProps[0].querySelector(".value").className.includes("token-other"),
      "Should have the right token class for '0'.");

    is(argsProps[1].querySelector(".name").getAttribute("value"), "1",
      "Should have the right property name for '1'.");
    is(argsProps[1].querySelector(".value").getAttribute("value"), "\"beta\"",
      "Should have the right property value for '1'.");
    ok(argsProps[1].querySelector(".value").className.includes("token-string"),
      "Should have the right token class for '1'.");

    is(argsProps[2].querySelector(".name").getAttribute("value"), "2",
      "Should have the right property name for '2'.");
    is(argsProps[2].querySelector(".value").getAttribute("value"), "3",
      "Should have the right property name for '2'.");
    ok(argsProps[2].querySelector(".value").className.includes("token-number"),
      "Should have the right token class for '2'.");

    is(argsProps[3].querySelector(".name").getAttribute("value"), "3",
      "Should have the right property name for '3'.");
    is(argsProps[3].querySelector(".value").getAttribute("value"), "false",
      "Should have the right property value for '3'.");
    ok(argsProps[3].querySelector(".value").className.includes("token-boolean"),
      "Should have the right token class for '3'.");

    is(argsProps[4].querySelector(".name").getAttribute("value"), "4",
      "Should have the right property name for '4'.");
    is(argsProps[4].querySelector(".value").getAttribute("value"), "null",
      "Should have the right property name for '4'.");
    ok(argsProps[4].querySelector(".value").className.includes("token-null"),
      "Should have the right token class for '4'.");

    is(gVariables.getItemForNode(argsProps[0]).target,
       argsVar.target.querySelectorAll(".variables-view-property")[0],
      "getItemForNode([0]) didn't return the expected property.");

    is(gVariables.getItemForNode(argsProps[1]).target,
       argsVar.target.querySelectorAll(".variables-view-property")[1],
      "getItemForNode([1]) didn't return the expected property.");

    is(gVariables.getItemForNode(argsProps[2]).target,
       argsVar.target.querySelectorAll(".variables-view-property")[2],
      "getItemForNode([2]) didn't return the expected property.");

    is(argsVar.find(argsProps[0]).target,
       argsVar.target.querySelectorAll(".variables-view-property")[0],
      "find([0]) didn't return the expected property.");

    is(argsVar.find(argsProps[1]).target,
       argsVar.target.querySelectorAll(".variables-view-property")[1],
      "find([1]) didn't return the expected property.");

    is(argsVar.find(argsProps[2]).target,
       argsVar.target.querySelectorAll(".variables-view-property")[2],
      "find([2]) didn't return the expected property.");

    let cProps = cVar.target.querySelectorAll(".variables-view-property");
    is(cProps.length, 7,
      "The 'c' variable should contain 6 enumerable and 1 non-enumerable properties");

    is(cProps[0].querySelector(".name").getAttribute("value"), "a",
      "Should have the right property name for 'a'.");
    is(cProps[0].querySelector(".value").getAttribute("value"), "1",
      "Should have the right property value for 'a'.");
    ok(cProps[0].querySelector(".value").className.includes("token-number"),
      "Should have the right token class for 'a'.");

    is(cProps[1].querySelector(".name").getAttribute("value"), "b",
      "Should have the right property name for 'b'.");
    is(cProps[1].querySelector(".value").getAttribute("value"), "\"beta\"",
      "Should have the right property value for 'b'.");
    ok(cProps[1].querySelector(".value").className.includes("token-string"),
      "Should have the right token class for 'b'.");

    is(cProps[2].querySelector(".name").getAttribute("value"), "c",
      "Should have the right property name for 'c'.");
    is(cProps[2].querySelector(".value").getAttribute("value"), "3",
      "Should have the right property value for 'c'.");
    ok(cProps[2].querySelector(".value").className.includes("token-number"),
      "Should have the right token class for 'c'.");

    is(cProps[3].querySelector(".name").getAttribute("value"), "d",
      "Should have the right property name for 'd'.");
    is(cProps[3].querySelector(".value").getAttribute("value"), "false",
      "Should have the right property value for 'd'.");
    ok(cProps[3].querySelector(".value").className.includes("token-boolean"),
      "Should have the right token class for 'd'.");

    is(cProps[4].querySelector(".name").getAttribute("value"), "e",
      "Should have the right property name for 'e'.");
    is(cProps[4].querySelector(".value").getAttribute("value"), "null",
      "Should have the right property value for 'e'.");
    ok(cProps[4].querySelector(".value").className.includes("token-null"),
      "Should have the right token class for 'e'.");

    is(cProps[5].querySelector(".name").getAttribute("value"), "f",
      "Should have the right property name for 'f'.");
    is(cProps[5].querySelector(".value").getAttribute("value"), "undefined",
      "Should have the right property value for 'f'.");
    ok(cProps[5].querySelector(".value").className.includes("token-undefined"),
      "Should have the right token class for 'f'.");

    is(gVariables.getItemForNode(cProps[0]).target,
       cVar.target.querySelectorAll(".variables-view-property")[0],
      "getItemForNode([0]) didn't return the expected property.");

    is(gVariables.getItemForNode(cProps[1]).target,
       cVar.target.querySelectorAll(".variables-view-property")[1],
      "getItemForNode([1]) didn't return the expected property.");

    is(gVariables.getItemForNode(cProps[2]).target,
       cVar.target.querySelectorAll(".variables-view-property")[2],
      "getItemForNode([2]) didn't return the expected property.");

    is(cVar.find(cProps[0]).target,
       cVar.target.querySelectorAll(".variables-view-property")[0],
      "find([0]) didn't return the expected property.");

    is(cVar.find(cProps[1]).target,
       cVar.target.querySelectorAll(".variables-view-property")[1],
      "find([1]) didn't return the expected property.");

    is(cVar.find(cProps[2]).target,
       cVar.target.querySelectorAll(".variables-view-property")[2],
      "find([2]) didn't return the expected property.");
  });

  // Expand the 'this', 'arguments' and 'c' variables view nodes. This causes
  // their properties to be retrieved and displayed.
  thisVar.expand();
  argsVar.expand();
  cVar.expand();

  is(thisVar.expanded, true,
    "The thisVar should be immediately marked as expanded.");
  is(argsVar.expanded, true,
    "The argsVar should be immediately marked as expanded.");
  is(cVar.expanded, true,
    "The cVar should be immediately marked as expanded.");
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gVariables = null;
});
