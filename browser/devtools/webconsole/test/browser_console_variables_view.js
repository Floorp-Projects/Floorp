/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that variables view works as expected in the web console.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-eval-in-stackframe.html";

let gWebConsole, gJSTerm, gVariablesView;

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
  gJSTerm.execute("fooObj", onExecuteFooObj);
}

function onExecuteFooObj()
{
  let msg = gWebConsole.outputNode.querySelector(".webconsole-msg-output");
  ok(msg, "output message found");
  isnot(msg.textContent.indexOf("[object Object]"), -1, "message text check");

  gJSTerm.once("variablesview-fetched", onFooObjFetch);

  executeSoon(() =>
    EventUtils.synthesizeMouse(msg, 2, 2, {}, gWebConsole.iframeWindow)
  );
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

  // Check that property value updates work and that jsterm functions can be
  // used.
  updateVariablesViewProperty({
    property: prop,
    field: "value",
    string: "document.title + window.location + $('p')",
    webconsole: gWebConsole,
    callback: onFooObjFetchAfterUpdate,
  });
}

function onFooObjFetchAfterUpdate(aEvent, aVar)
{
  info("onFooObjFetchAfterUpdate");
  let para = content.document.querySelector("p");
  let expectedValue = content.document.title + content.location + para;

  findVariableViewProperties(aVar, [
    { name: "testProp", value: expectedValue },
  ], { webconsole: gWebConsole }).then(onUpdatedTestPropFound);
}

function onUpdatedTestPropFound(aResults)
{
  let prop = aResults[0].matchedProp;
  ok(prop, "matched the updated |testProp| property value");

  is(content.wrappedJSObject.fooObj.testProp, aResults[0].value,
     "|fooObj.testProp| value has been updated");

  // Check that property name updates work.
  updateVariablesViewProperty({
    property: prop,
    field: "name",
    string: "testUpdatedProp",
    webconsole: gWebConsole,
    callback: onFooObjFetchAfterPropRename,
  });
}

function onFooObjFetchAfterPropRename(aEvent, aVar)
{
  info("onFooObjFetchAfterPropRename");

  let para = content.document.querySelector("p");
  let expectedValue = content.document.title + content.location + para;

  // Check that the new value is in the variables view.
  findVariableViewProperties(aVar, [
    { name: "testUpdatedProp", value: expectedValue },
  ], { webconsole: gWebConsole }).then(onRenamedTestPropFound);
}

function onRenamedTestPropFound(aResults)
{
  let prop = aResults[0].matchedProp;
  ok(prop, "matched the renamed |testProp| property");

  ok(!content.wrappedJSObject.fooObj.testProp,
     "|fooObj.testProp| has been deleted");
  is(content.wrappedJSObject.fooObj.testUpdatedProp, aResults[0].value,
     "|fooObj.testUpdatedProp| is correct");

  // Check that property value updates that cause exceptions are reported in
  // the web console output.
  updateVariablesViewProperty({
    property: prop,
    field: "value",
    string: "foobarzFailure()",
    webconsole: gWebConsole,
    callback: onPropUpdateError,
  });
}

function onPropUpdateError(aEvent, aVar)
{
  info("onPropUpdateError");

  let para = content.document.querySelector("p");
  let expectedValue = content.document.title + content.location + para;

  // Make sure the property did not change.
  findVariableViewProperties(aVar, [
    { name: "testUpdatedProp", value: expectedValue },
  ], { webconsole: gWebConsole }).then(onRenamedTestPropFoundAgain);
}

function onRenamedTestPropFoundAgain(aResults)
{
  let prop = aResults[0].matchedProp;
  ok(prop, "matched the renamed |testProp| property again");

  let outputNode = gWebConsole.outputNode;

  waitForSuccess({
    name: "exception in property update reported in the web console output",
    validatorFn: () => outputNode.textContent.indexOf("foobarzFailure") != -1,
    successFn: testPropDelete.bind(null, prop),
    failureFn: testPropDelete.bind(null, prop),
  });
}

function testPropDelete(aProp)
{
  gVariablesView.window.focus();
  aProp.focus();

  executeSoon(() => {
    EventUtils.synthesizeKey("VK_DELETE", {}, gVariablesView.window);
    gWebConsole = gJSTerm = gVariablesView = null;
  });

  waitForSuccess({
    name: "property deleted",
    validatorFn: () => !("testUpdatedProp" in content.wrappedJSObject.fooObj),
    successFn: finishTest,
    failureFn: finishTest,
  });
}
