/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that the variables view sidebar can be closed by pressing Escape in the
// web console.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-eval-in-stackframe.html";

let gWebConsole, gJSTerm, gVariablesView;

function test()
{
  registerCleanupFunction(() => {
    gWebConsole = gJSTerm = gVariablesView = null;
  });

  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(hud)
{
  gWebConsole = hud;
  gJSTerm = hud.jsterm;
  gJSTerm.execute("fooObj", onExecuteFooObj);
}

function onExecuteFooObj()
{
  let msg = gWebConsole.outputNode.querySelector(".webconsole-msg-output");
  ok(msg, "output message found");
  isnot(msg.textContent.indexOf("[object Object]"), -1, "message text check");

  gJSTerm.once("variablesview-fetched", onFooObjFetch);
  EventUtils.synthesizeMouse(msg, 2, 2, {}, gWebConsole.iframeWindow)
}

function onFooObjFetch(aEvent, aVar)
{
  gVariablesView = aVar._variablesView;
  ok(gVariablesView, "variables view object");

  findVariableViewProperties(aVar, [
    { name: "testProp", value: "testValue" },
  ], { webconsole: gWebConsole }).then(onTestPropFound);
}

function onTestPropFound(aResults)
{
  let prop = aResults[0].matchedProp;
  ok(prop, "matched the |testProp| property in the variables view");

  is(content.wrappedJSObject.fooObj.testProp, aResults[0].value,
     "|fooObj.testProp| value is correct");

  gVariablesView.window.focus();
  gJSTerm.once("sidebar-closed", onSidebarClosed);
  EventUtils.synthesizeKey("VK_ESCAPE", {}, gVariablesView.window);
}

function onSidebarClosed()
{
  gJSTerm.clearOutput();
  gJSTerm.execute("window", onExecuteWindow);
}

function onExecuteWindow()
{
  let msg = gWebConsole.outputNode.querySelector(".webconsole-msg-output");
  ok(msg, "output message found");
  isnot(msg.textContent.indexOf("[object Window]"), -1, "message text check");

  gJSTerm.once("variablesview-fetched", onWindowFetch);
  EventUtils.synthesizeMouse(msg, 2, 2, {}, gWebConsole.iframeWindow)
}

function onWindowFetch(aEvent, aVar)
{
  gVariablesView = aVar._variablesView;
  ok(gVariablesView, "variables view object");

  findVariableViewProperties(aVar, [
    { name: "foo", value: "globalFooBug783499" },
  ], { webconsole: gWebConsole }).then(onFooFound);
}

function onFooFound(aResults)
{
  gVariablesView.window.focus();
  gJSTerm.once("sidebar-closed", finishTest);
  EventUtils.synthesizeKey("VK_ESCAPE", {}, gVariablesView.window);
}

