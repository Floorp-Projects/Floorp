/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that makes sure web console eval happens in the user-selected stackframe
// from the js debugger, when changing the value of a property in the variables
// view.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-eval-in-stackframe.html";

let gWebConsole, gJSTerm, gDebuggerWin, gThread, gDebuggerController,
    gStackframes, gVariablesView;

function test()
{
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

  executeSoon(() => {
    info("openDebugger");
    openDebugger().then(debuggerOpened);
  });
}

function debuggerOpened(aResult)
{
  gDebuggerWin = aResult.panelWin;
  gDebuggerController = gDebuggerWin.DebuggerController;
  gThread = gDebuggerController.activeThread;
  gStackframes = gDebuggerController.StackFrames;

  executeSoon(() => {
    gThread.addOneTimeListener("framesadded", onFramesAdded);

    info("firstCall()");
    content.wrappedJSObject.firstCall();
  });
}

function onFramesAdded()
{
  info("onFramesAdded");

  executeSoon(() =>
    openConsole(null, () =>
      gJSTerm.execute("fooObj", onExecuteFooObj)
    )
  );
}


function onExecuteFooObj(msg)
{
  ok(msg, "output message found");
  ok(msg.textContent.contains('{ testProp2: "testValue2" }'), "message text check");

  let anchor = msg.querySelector("a");
  ok(anchor, "object link found");

  gJSTerm.once("variablesview-fetched", onFooObjFetch);

  executeSoon(() => EventUtils.synthesizeMouse(anchor, 2, 2, {},
                                               gWebConsole.iframeWindow));
}

function onFooObjFetch(aEvent, aVar)
{
  gVariablesView = aVar._variablesView;
  ok(gVariablesView, "variables view object");

  findVariableViewProperties(aVar, [
    { name: "testProp2", value: "testValue2" },
    { name: "testProp", value: "testValue", dontMatch: true },
  ], { webconsole: gWebConsole }).then(onTestPropFound);
}

function onTestPropFound(aResults)
{
  let prop = aResults[0].matchedProp;
  ok(prop, "matched the |testProp2| property in the variables view");

  // Check that property value updates work and that jsterm functions can be
  // used.
  updateVariablesViewProperty({
    property: prop,
    field: "value",
    string: "document.title + foo2 + $('p')",
    webconsole: gWebConsole,
    callback: onFooObjFetchAfterUpdate,
  });
}

function onFooObjFetchAfterUpdate(aEvent, aVar)
{
  info("onFooObjFetchAfterUpdate");
  let para = content.wrappedJSObject.document.querySelector("p");
  let expectedValue = content.document.title + "foo2SecondCall" + para;

  findVariableViewProperties(aVar, [
    { name: "testProp2", value: expectedValue },
  ], { webconsole: gWebConsole }).then(onUpdatedTestPropFound);
}

function onUpdatedTestPropFound(aResults)
{
  let prop = aResults[0].matchedProp;
  ok(prop, "matched the updated |testProp2| property value");

  // Check that testProp2 was updated.
  executeSoon(() => gJSTerm.execute("fooObj.testProp2", onExecuteFooObjTestProp2));
}

function onExecuteFooObjTestProp2()
{
  let para = content.wrappedJSObject.document.querySelector("p");
  let expected = content.document.title + "foo2SecondCall" + para;

  isnot(gWebConsole.outputNode.textContent.indexOf(expected), -1,
        "fooObj.testProp2 is correct");

  gWebConsole = gJSTerm = gDebuggerWin = gThread = gDebuggerController =
    gStackframes = gVariablesView = null;
  executeSoon(finishTest);
}
