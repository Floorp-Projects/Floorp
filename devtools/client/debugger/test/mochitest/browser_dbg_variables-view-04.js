/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that grips are correctly applied to variables.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    let variables = aPanel.panelWin.DebuggerView.Variables;
    let testScope = variables.addScope("test");
    let testVar = testScope.addItem("something");

    testVar.setGrip(1.618);

    is(testVar.target.querySelector(".value").getAttribute("value"), "1.618",
      "The grip information for the variable wasn't set correctly (1).");
    is(testVar.target.querySelector(".variables-view-element-details.enum").childNodes.length, 0,
      "Setting the grip alone shouldn't add any new tree nodes (1).");
    is(testVar.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "Setting the grip alone shouldn't add any new tree nodes (2).");

    testVar.setGrip({
      type: "object",
      class: "Window"
    });

    is(testVar.target.querySelector(".value").getAttribute("value"), "Window",
      "The grip information for the variable wasn't set correctly (2).");
    is(testVar.target.querySelector(".variables-view-element-details.enum").childNodes.length, 0,
      "Setting the grip alone shouldn't add any new tree nodes (3).");
    is(testVar.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "Setting the grip alone shouldn't add any new tree nodes (4).");

    testVar.addItems({
      helloWorld: {
        value: "hello world",
        enumerable: true
      }
    });

    is(testVar.target.querySelector(".variables-view-element-details").childNodes.length, 1,
      "A new detail node should have been added in the variable tree.");
    is(testVar.get("helloWorld").target.querySelector(".value").getAttribute("value"), "\"hello world\"",
      "The grip information for the variable wasn't set correctly (3).");

    testVar.addItems({
      helloWorld: {
        value: "hello jupiter",
        enumerable: true
      }
    });

    is(testVar.target.querySelector(".variables-view-element-details").childNodes.length, 1,
      "Shouldn't be able to duplicate nodes added in the variable tree.");
    is(testVar.get("helloWorld").target.querySelector(".value").getAttribute("value"), "\"hello world\"",
      "The grip information for the variable wasn't preserved correctly (4).");

    testVar.addItems({
      someProp0: {
        value: "random string",
        enumerable: true
      },
      someProp1: {
        value: "another string",
        enumerable: true
      }
    });

    is(testVar.target.querySelector(".variables-view-element-details").childNodes.length, 3,
      "Two new detail nodes should have been added in the variable tree.");
    is(testVar.get("someProp0").target.querySelector(".value").getAttribute("value"), "\"random string\"",
      "The grip information for the variable wasn't set correctly (5).");
    is(testVar.get("someProp1").target.querySelector(".value").getAttribute("value"), "\"another string\"",
      "The grip information for the variable wasn't set correctly (6).");

    testVar.addItems({
      someProp2: {
        value: {
          type: "null"
        },
        enumerable: true
      },
      someProp3: {
        value: {
          type: "undefined"
        },
        enumerable: true
      },
      someProp4: {
        value: {
          type: "object",
          class: "Object"
        },
        enumerable: true
      }
    });

    is(testVar.target.querySelector(".variables-view-element-details").childNodes.length, 6,
      "Three new detail nodes should have been added in the variable tree.");
    is(testVar.get("someProp2").target.querySelector(".value").getAttribute("value"), "null",
      "The grip information for the variable wasn't set correctly (7).");
    is(testVar.get("someProp3").target.querySelector(".value").getAttribute("value"), "undefined",
      "The grip information for the variable wasn't set correctly (8).");
    is(testVar.get("someProp4").target.querySelector(".value").getAttribute("value"), "Object",
      "The grip information for the variable wasn't set correctly (9).");

    let parent = testVar.get("someProp2");
    let child = parent.addItem("child", {
      value: {
        type: "null"
      }
    });

    is(variables.getItemForNode(parent.target), parent,
       "VariablesView should have a record of the parent.");
    is(variables.getItemForNode(child.target), child,
       "VariablesView should have a record of the child.");
    is([...parent].length, 1,
       "Parent should have one child.");

    parent.remove();

    is(variables.getItemForNode(parent.target), undefined,
       "VariablesView should not have a record of the parent anymore.");
    is(parent.target.parentNode, null,
       "Parent element should not have a parent.");
    is(variables.getItemForNode(child.target), undefined,
       "VariablesView should not have a record of the child anymore.");
    is(child.target.parentNode, null,
       "Child element should not have a parent.");
    is([...parent].length, 0,
       "Parent should have zero children.");

    testScope.remove();

    is([...variables].length, 0,
       "VariablesView should have been emptied.");
    is(ThreadSafeChromeUtils.nondeterministicGetWeakMapKeys(variables._itemsByElement).length,
       0, "VariablesView _itemsByElement map has been emptied.");
    is(variables._currHierarchy.size, 0,
       "VariablesView _currHierarchy map has been emptied.");
    is(variables._list.children.length, 0,
       "VariablesView element should have no children.");

    closeDebuggerAndFinish(aPanel);
  });
}
