/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that creating, collapsing and expanding variables in the
 * variables view works as expected.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    let variables = aPanel.panelWin.DebuggerView.Variables;
    let testScope = variables.addScope("test");
    let testVar = testScope.addItem("something");
    let duplVar = testScope.addItem("something");

    info("Scope id: " + testScope.id);
    info("Scope name: " + testScope.name);
    info("Variable id: " + testVar.id);
    info("Variable name: " + testVar.name);

    ok(testScope,
      "Should have created a scope.");
    is(duplVar, testVar,
      "Shouldn't be able to duplicate variables in the same scope.");

    ok(testVar,
      "Should have created a variable.");
    ok(testVar.id.includes("something"),
      "The newly created variable should have the default id set.");
    is(testVar.name, "something",
      "The newly created variable should have the desired name set.");

    ok(!testVar.displayValue,
      "The newly created variable should not have a displayed value yet (1).");
    ok(!testVar.displayValueClassName,
      "The newly created variable should not have a displayed value yet (2).");

    ok(testVar.target,
      "The newly created scope should point to a target node.");
    ok(testVar.target.id.includes("something"),
      "Should have the correct variable id on the element.");

    is(testVar.target.querySelector(".name").getAttribute("value"), "something",
      "Any new variable should have the designated name.");
    is(testVar.target.querySelector(".variables-view-element-details.enum").childNodes.length, 0,
      "Any new variable should have a container with no enumerable child nodes.");
    is(testVar.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "Any new variable should have a container with no non-enumerable child nodes.");

    ok(!testVar.expanded,
      "Any new created scope should be initially collapsed.");
    ok(testVar.visible,
      "Any new created scope should be initially visible.");

    let expandCallbackArg = null;
    let collapseCallbackArg = null;
    let toggleCallbackArg = null;
    let hideCallbackArg = null;
    let showCallbackArg = null;

    testVar.onexpand = aScope => expandCallbackArg = aScope;
    testVar.oncollapse = aScope => collapseCallbackArg = aScope;
    testVar.ontoggle = aScope => toggleCallbackArg = aScope;
    testVar.onhide = aScope => hideCallbackArg = aScope;
    testVar.onshow = aScope => showCallbackArg = aScope;

    testVar.expand();
    ok(testVar.expanded,
      "The testVar shouldn't be collapsed anymore.");
    is(expandCallbackArg, testVar,
      "The expandCallback wasn't called as it should.");

    testVar.collapse();
    ok(!testVar.expanded,
      "The testVar should be collapsed again.");
    is(collapseCallbackArg, testVar,
      "The collapseCallback wasn't called as it should.");

    testVar.expanded = true;
    ok(testVar.expanded,
      "The testVar shouldn't be collapsed anymore.");

    testVar.toggle();
    ok(!testVar.expanded,
      "The testVar should be collapsed again.");
    is(toggleCallbackArg, testVar,
      "The toggleCallback wasn't called as it should.");

    testVar.hide();
    ok(!testVar.visible,
      "The testVar should be invisible after hiding.");
    is(hideCallbackArg, testVar,
      "The hideCallback wasn't called as it should.");

    testVar.show();
    ok(testVar.visible,
      "The testVar should be visible again.");
    is(showCallbackArg, testVar,
      "The showCallback wasn't called as it should.");

    testVar.visible = false;
    ok(!testVar.visible,
      "The testVar should be invisible after hiding.");
    ok(!testVar.expanded,
      "The testVar should remember it is collapsed even if it is hidden.");

    testVar.visible = true;
    ok(testVar.visible,
      "The testVar should be visible after reshowing.");
    ok(!testVar.expanded,
      "The testVar should remember it is collapsed after it is reshown.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      testVar.target.querySelector(".name"),
      aPanel.panelWin);

    ok(testVar.expanded,
      "Clicking the testVar name should expand it.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      testVar.target.querySelector(".name"),
      aPanel.panelWin);

    ok(!testVar.expanded,
      "Clicking again the testVar name should collapse it.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      testVar.target.querySelector(".arrow"),
      aPanel.panelWin);

    ok(testVar.expanded,
      "Clicking the testVar arrow should expand it.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      testVar.target.querySelector(".arrow"),
      aPanel.panelWin);

    ok(!testVar.expanded,
      "Clicking again the testVar arrow should collapse it.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      testVar.target.querySelector(".title"),
      aPanel.panelWin);

    ok(testVar.expanded,
      "Clicking the testVar title should expand it again.");

    testVar.addItem("child", {
      value: {
        type: "object",
        class: "Object"
      }
    });

    let testChild = testVar.get("child");
    ok(testChild,
      "Should have created a child property.");
    ok(testChild.id.includes("child"),
      "The newly created property should have the default id set.");
    is(testChild.name, "child",
      "The newly created property should have the desired name set.");

    is(testChild.displayValue, "Object",
      "The newly created property should not have a displayed value yet (1).");
    is(testChild.displayValueClassName, "token-other",
      "The newly created property should not have a displayed value yet (2).");

    ok(testChild.target,
      "The newly created scope should point to a target node.");
    ok(testChild.target.id.includes("child"),
      "Should have the correct property id on the element.");

    is(testChild.target.querySelector(".name").getAttribute("value"), "child",
      "Any new property should have the designated name.");
    is(testChild.target.querySelector(".variables-view-element-details.enum").childNodes.length, 0,
      "Any new property should have a container with no enumerable child nodes.");
    is(testChild.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "Any new property should have a container with no non-enumerable child nodes.");

    ok(!testChild.expanded,
      "Any new created scope should be initially collapsed.");
    ok(testChild.visible,
      "Any new created scope should be initially visible.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      testChild.target.querySelector(".name"),
      aPanel.panelWin);

    ok(testChild.expanded,
      "Clicking the testChild name should expand it.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      testChild.target.querySelector(".name"),
      aPanel.panelWin);

    ok(!testChild.expanded,
      "Clicking again the testChild name should collapse it.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      testChild.target.querySelector(".arrow"),
      aPanel.panelWin);

    ok(testChild.expanded,
      "Clicking the testChild arrow should expand it.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      testChild.target.querySelector(".arrow"),
      aPanel.panelWin);

    ok(!testChild.expanded,
      "Clicking again the testChild arrow should collapse it.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      testChild.target.querySelector(".title"),
      aPanel.panelWin);

    closeDebuggerAndFinish(aPanel);
  });
}
