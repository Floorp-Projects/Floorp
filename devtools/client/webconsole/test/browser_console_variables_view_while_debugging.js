/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that makes sure web console eval happens in the user-selected stackframe
// from the js debugger, when changing the value of a property in the variables
// view.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-eval-in-stackframe.html";

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  let dbgPanel = yield openDebugger();
  yield waitForFrameAdded(dbgPanel);
  yield openConsole();
  yield testVariablesView(hud);
});

function* waitForFrameAdded(dbgPanel) {
  let thread = dbgPanel.panelWin.DebuggerController.activeThread;

  info("Waiting for framesadded");
  yield new Promise(resolve => {
    thread.addOneTimeListener("framesadded", resolve);
    ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
      content.wrappedJSObject.firstCall();
    });
  });
}

function* testVariablesView(hud) {
  let jsterm = hud.jsterm;
  let msg = yield jsterm.execute("fooObj");
  ok(msg, "output message found");
  ok(msg.textContent.includes('{ testProp2: "testValue2" }'),
                              "message text check");

  let anchor = msg.querySelector("a");
  ok(anchor, "object link found");

  info("Waiting for variable view to appear");
  let variable = yield new Promise(resolve => {
    jsterm.once("variablesview-fetched", (e, variable) => {
      resolve(variable);
    });
    executeSoon(() => EventUtils.synthesizeMouse(anchor, 2, 2, {},
                                                 hud.iframeWindow));
  });

  info("Waiting for findVariableViewProperties");
  let results = yield findVariableViewProperties(variable, [
    { name: "testProp2", value: "testValue2" },
    { name: "testProp", value: "testValue", dontMatch: true },
  ], { webconsole: hud });

  let prop = results[0].matchedProp;
  ok(prop, "matched the |testProp2| property in the variables view");

  // Check that property value updates work and that jsterm functions can be
  // used.
  variable = yield updateVariablesViewProperty({
    property: prop,
    field: "value",
    string: "document.title + foo2 + $('p')",
    webconsole: hud
  });

  info("onFooObjFetchAfterUpdate");
  let expectedValue = yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    let para = content.wrappedJSObject.document.querySelector("p");
    return content.document.title + "foo2SecondCall" + para;
  });

  results = yield findVariableViewProperties(variable, [
    { name: "testProp2", value: expectedValue },
  ], { webconsole: hud });

  prop = results[0].matchedProp;
  ok(prop, "matched the updated |testProp2| property value");

  // Check that testProp2 was updated.
  yield new Promise(resolve => {
    executeSoon(() => {
      jsterm.execute("fooObj.testProp2").then(resolve);
    });
  });

  expectedValue = yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    let para = content.wrappedJSObject.document.querySelector("p");
    return content.document.title + "foo2SecondCall" + para;
  });

  isnot(hud.outputNode.textContent.indexOf(expectedValue), -1,
        "fooObj.testProp2 is correct");
}
