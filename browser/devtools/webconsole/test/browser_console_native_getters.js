/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that native getters and setters for DOM elements work as expected in
// variables view - bug 870220.

const TEST_URI = "data:text/html;charset=utf8,<title>bug870220</title>\n" +
                 "<p>hello world\n<p>native getters!";

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

  gJSTerm.execute("document");

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "HTMLDocument \u2192 data:text/html;charset=utf8",
      category: CATEGORY_OUTPUT,
      objects: true,
    }],
  }).then(onEvalResult);
}

function onEvalResult(aResults)
{
  let clickable = aResults[0].clickableElements[0];
  ok(clickable, "clickable object found");

  gJSTerm.once("variablesview-fetched", onDocumentFetch);
  EventUtils.synthesizeMouse(clickable, 2, 2, {}, gWebConsole.iframeWindow)
}

function onDocumentFetch(aEvent, aVar)
{
  gVariablesView = aVar._variablesView;
  ok(gVariablesView, "variables view object");

  findVariableViewProperties(aVar, [
    { name: "title", value: "bug870220" },
    { name: "bgColor" },
  ], { webconsole: gWebConsole }).then(onDocumentPropsFound);
}

function onDocumentPropsFound(aResults)
{
  let prop = aResults[1].matchedProp;
  ok(prop, "matched the |bgColor| property in the variables view");

  // Check that property value updates work.
  updateVariablesViewProperty({
    property: prop,
    field: "value",
    string: "'red'",
    webconsole: gWebConsole,
    callback: onFetchAfterBackgroundUpdate,
  });
}

function onFetchAfterBackgroundUpdate(aEvent, aVar)
{
  info("onFetchAfterBackgroundUpdate");

  is(content.document.bgColor, "red", "document background color changed");

  findVariableViewProperties(aVar, [
    { name: "bgColor", value: "red" },
  ], { webconsole: gWebConsole }).then(testParagraphs);
}

function testParagraphs()
{
  gJSTerm.execute("$$('p')");

  waitForMessages({
    webconsole: gWebConsole,
    messages: [{
      text: "NodeList [",
      category: CATEGORY_OUTPUT,
      objects: true,
    }],
  }).then(onEvalNodeList);
}

function onEvalNodeList(aResults)
{
  let clickable = aResults[0].clickableElements[0];
  ok(clickable, "clickable object found");

  gJSTerm.once("variablesview-fetched", onNodeListFetch);
  EventUtils.synthesizeMouse(clickable, 2, 2, {}, gWebConsole.iframeWindow)
}

function onNodeListFetch(aEvent, aVar)
{
  gVariablesView = aVar._variablesView;
  ok(gVariablesView, "variables view object");

  findVariableViewProperties(aVar, [
    { name: "0.textContent", value: /hello world/ },
    { name: "1.textContent", value: /native getters/ },
  ], { webconsole: gWebConsole }).then(() => {
    gWebConsole = gJSTerm = gVariablesView = null;
    finishTest();
  });
}
