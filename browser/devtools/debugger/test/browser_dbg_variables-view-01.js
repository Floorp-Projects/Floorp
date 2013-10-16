/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that creating, collpasing and expanding scopes in the
 * variables view works as expected.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    let variables = aPanel.panelWin.DebuggerView.Variables;
    let testScope = variables.addScope("test");

    ok(testScope,
      "Should have created a scope.");
    ok(testScope.id.contains("test"),
      "The newly created scope should have the default id set.");
    is(testScope.name, "test",
      "The newly created scope should have the desired name set.");

    ok(!testScope.displayValue,
      "The newly created scope should not have a displayed value (1).");
    ok(!testScope.displayValueClassName,
      "The newly created scope should not have a displayed value (2).");

    ok(testScope.target,
      "The newly created scope should point to a target node.");
    ok(testScope.target.id.contains("test"),
      "Should have the correct scope id on the element.");

    is(testScope.target.querySelector(".name").getAttribute("value"), "test",
      "Any new scope should have the designated name.");
    is(testScope.target.querySelector(".variables-view-element-details.enum").childNodes.length, 0,
      "Any new scope should have a container with no enumerable child nodes.");
    is(testScope.target.querySelector(".variables-view-element-details.nonenum").childNodes.length, 0,
      "Any new scope should have a container with no non-enumerable child nodes.");

    ok(!testScope.expanded,
      "Any new created scope should be initially collapsed.");
    ok(testScope.visible,
      "Any new created scope should be initially visible.");

    let expandCallbackArg = null;
    let collapseCallbackArg = null;
    let toggleCallbackArg = null;
    let hideCallbackArg = null;
    let showCallbackArg = null;

    testScope.onexpand = aScope => expandCallbackArg = aScope;
    testScope.oncollapse = aScope => collapseCallbackArg = aScope;
    testScope.ontoggle = aScope => toggleCallbackArg = aScope;
    testScope.onhide = aScope => hideCallbackArg = aScope;
    testScope.onshow = aScope => showCallbackArg = aScope;

    testScope.expand();
    ok(testScope.expanded,
      "The testScope shouldn't be collapsed anymore.");
    is(expandCallbackArg, testScope,
      "The expandCallback wasn't called as it should.");

    testScope.collapse();
    ok(!testScope.expanded,
      "The testScope should be collapsed again.");
    is(collapseCallbackArg, testScope,
      "The collapseCallback wasn't called as it should.");

    testScope.expanded = true;
    ok(testScope.expanded,
      "The testScope shouldn't be collapsed anymore.");

    testScope.toggle();
    ok(!testScope.expanded,
      "The testScope should be collapsed again.");
    is(toggleCallbackArg, testScope,
      "The toggleCallback wasn't called as it should.");

    testScope.hide();
    ok(!testScope.visible,
      "The testScope should be invisible after hiding.");
    is(hideCallbackArg, testScope,
      "The hideCallback wasn't called as it should.");

    testScope.show();
    ok(testScope.visible,
      "The testScope should be visible again.");
    is(showCallbackArg, testScope,
      "The showCallback wasn't called as it should.");

    testScope.visible = false;
    ok(!testScope.visible,
      "The testScope should be invisible after hiding.");
    ok(!testScope.expanded,
      "The testScope should remember it is collapsed even if it is hidden.");

    testScope.visible = true;
    ok(testScope.visible,
      "The testScope should be visible after reshowing.");
    ok(!testScope.expanded,
      "The testScope should remember it is collapsed after it is reshown.");

    EventUtils.sendMouseEvent({ type: "mousedown", button: 1 },
      testScope.target.querySelector(".title"),
      aPanel.panelWin);

    ok(!testScope.expanded,
      "Clicking the testScope title with the right mouse button should't expand it.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      testScope.target.querySelector(".title"),
      aPanel.panelWin);

    ok(testScope.expanded,
      "Clicking the testScope title should expand it.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      testScope.target.querySelector(".title"),
      aPanel.panelWin);

    ok(!testScope.expanded,
      "Clicking again the testScope title should collapse it.");

    closeDebuggerAndFinish(aPanel);
  });
}
