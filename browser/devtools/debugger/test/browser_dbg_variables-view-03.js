/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that recursively creating properties in the variables view works
 * as expected.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    let variables = aPanel.panelWin.DebuggerView.Variables;
    let testScope = variables.addScope("test");

    is(testScope.target.querySelectorAll(".variables-view-element-details.enum").length, 1,
      "One enumerable container should be present in the scope.");
    is(testScope.target.querySelectorAll(".variables-view-element-details.nonenum").length, 1,
      "One non-enumerable container should be present in the scope.");
    is(testScope.target.querySelector(".variables-view-element-details.enum").childNodes.length, 0,
      "No enumerable variables should be present in the scope.");
    is(testScope.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "No non-enumerable variables should be present in the scope.");

    testScope.addItem("something", {
      value: {
        type: "object",
        class: "Object"
      },
      enumerable: true
    });

    is(testScope.target.querySelectorAll(".variables-view-element-details.enum").length, 2,
      "Two enumerable containers should be present in the tree.");
    is(testScope.target.querySelectorAll(".variables-view-element-details.nonenum").length, 2,
      "Two non-enumerable containers should be present in the tree.");

    is(testScope.target.querySelector(".variables-view-element-details.enum").childNodes.length, 1,
      "A new enumerable variable should have been added in the scope.");
    is(testScope.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "No new non-enumerable variables should have been added in the scope.");

    let testVar = testScope.get("something");
    ok(testVar,
      "The added variable should be accessible from the scope.");

    is(testVar.target.querySelectorAll(".variables-view-element-details.enum").length, 1,
      "One enumerable container should be present in the variable.");
    is(testVar.target.querySelectorAll(".variables-view-element-details.nonenum").length, 1,
      "One non-enumerable container should be present in the variable.");
    is(testVar.target.querySelector(".variables-view-element-details.enum").childNodes.length, 0,
      "No enumerable properties should be present in the variable.");
    is(testVar.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "No non-enumerable properties should be present in the variable.");

    testVar.addItem("child", {
      value: {
        type: "object",
        class: "Object"
      },
      enumerable: true
    });

    is(testScope.target.querySelectorAll(".variables-view-element-details.enum").length, 3,
      "Three enumerable containers should be present in the tree.");
    is(testScope.target.querySelectorAll(".variables-view-element-details.nonenum").length, 3,
      "Three non-enumerable containers should be present in the tree.");

    is(testVar.target.querySelector(".variables-view-element-details.enum").childNodes.length, 1,
      "A new enumerable property should have been added in the variable.");
    is(testVar.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "No new non-enumerable properties should have been added in the variable.");

    let testChild = testVar.get("child");
    ok(testChild,
      "The added property should be accessible from the variable.");

    is(testChild.target.querySelectorAll(".variables-view-element-details.enum").length, 1,
      "One enumerable container should be present in the property.");
    is(testChild.target.querySelectorAll(".variables-view-element-details.nonenum").length, 1,
      "One non-enumerable container should be present in the property.");
    is(testChild.target.querySelector(".variables-view-element-details.enum").childNodes.length, 0,
      "No enumerable sub-properties should be present in the property.");
    is(testChild.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "No non-enumerable sub-properties should be present in the property.");

    testChild.addItem("grandChild", {
      value: {
        type: "object",
        class: "Object"
      },
      enumerable: true
    });

    is(testScope.target.querySelectorAll(".variables-view-element-details.enum").length, 4,
      "Four enumerable containers should be present in the tree.");
    is(testScope.target.querySelectorAll(".variables-view-element-details.nonenum").length, 4,
      "Four non-enumerable containers should be present in the tree.");

    is(testChild.target.querySelector(".variables-view-element-details.enum").childNodes.length, 1,
      "A new enumerable sub-property should have been added in the property.");
    is(testChild.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "No new non-enumerable sub-properties should have been added in the property.");

    let testGrandChild = testChild.get("grandChild");
    ok(testGrandChild,
      "The added sub-property should be accessible from the property.");

    is(testGrandChild.target.querySelectorAll(".variables-view-element-details.enum").length, 1,
      "One enumerable container should be present in the property.");
    is(testGrandChild.target.querySelectorAll(".variables-view-element-details.nonenum").length, 1,
      "One non-enumerable container should be present in the property.");
    is(testGrandChild.target.querySelector(".variables-view-element-details.enum").childNodes.length, 0,
      "No enumerable sub-properties should be present in the property.");
    is(testGrandChild.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "No non-enumerable sub-properties should be present in the property.");

    testGrandChild.addItem("granderChild", {
      value: {
        type: "object",
        class: "Object"
      },
      enumerable: true
    });

    is(testScope.target.querySelectorAll(".variables-view-element-details.enum").length, 5,
      "Five enumerable containers should be present in the tree.");
    is(testScope.target.querySelectorAll(".variables-view-element-details.nonenum").length, 5,
      "Five non-enumerable containers should be present in the tree.");

    is(testGrandChild.target.querySelector(".variables-view-element-details.enum").childNodes.length, 1,
      "A new enumerable variable should have been added in the variable.");
    is(testGrandChild.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "No new non-enumerable variables should have been added in the variable.");

    let testGranderChild = testGrandChild.get("granderChild");
    ok(testGranderChild,
      "The added sub-property should be accessible from the property.");

    is(testGranderChild.target.querySelectorAll(".variables-view-element-details.enum").length, 1,
      "One enumerable container should be present in the property.");
    is(testGranderChild.target.querySelectorAll(".variables-view-element-details.nonenum").length, 1,
      "One non-enumerable container should be present in the property.");
    is(testGranderChild.target.querySelector(".variables-view-element-details.enum").childNodes.length, 0,
      "No enumerable sub-properties should be present in the property.");
    is(testGranderChild.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "No non-enumerable sub-properties should be present in the property.");

    closeDebuggerAndFinish(aPanel);
  });
}
