/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that users can inspect objects logged from cross-domain iframes -
// bug 869003.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-869003-top-window.html";

let gWebConsole, gJSTerm, gVariablesView;

function test()
{
  // This test is slightly more involved: it opens the web console, then the
  // variables view for a given object, it updates a property in the view and
  // checks the result. We can get a timeout with debug builds on slower machines.
  requestLongerTimeout(2);

  addTab("data:text/html;charset=utf8,<p>hello");
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(hud)
{
  gWebConsole = hud;
  gJSTerm = hud.jsterm;
  content.location = TEST_URI;

  waitForMessages({
    webconsole: hud,
    messages: [{
      name: "console.log message",
      text: "foobar",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      objects: true,
    }],
  }).then(onConsoleMessage);
}

function onConsoleMessage(aResults)
{
  let msg = [...aResults[0].matched][0];
  ok(msg, "message element");

  let body = msg.querySelector(".message-body");
  ok(body, "message body");

  let clickable = aResults[0].clickableElements[0];
  ok(clickable, "clickable object found");
  ok(body.textContent.contains('{ hello: "world!",'), "message text check");

  gJSTerm.once("variablesview-fetched", onObjFetch);

  EventUtils.synthesizeMouse(clickable, 2, 2, {}, gWebConsole.iframeWindow)
}

function onObjFetch(aEvent, aVar)
{
  gVariablesView = aVar._variablesView;
  ok(gVariablesView, "variables view object");

  findVariableViewProperties(aVar, [
    { name: "hello", value: "world!" },
    { name: "bug", value: 869003 },
  ], { webconsole: gWebConsole }).then(onPropFound);
}

function onPropFound(aResults)
{
  let prop = aResults[0].matchedProp;
  ok(prop, "matched the |hello| property in the variables view");

  // Check that property value updates work.
  updateVariablesViewProperty({
    property: prop,
    field: "value",
    string: "'omgtest'",
    webconsole: gWebConsole,
    callback: onFetchAfterUpdate,
  });
}

function onFetchAfterUpdate(aEvent, aVar)
{
  info("onFetchAfterUpdate");

  findVariableViewProperties(aVar, [
    { name: "hello", value: "omgtest" },
    { name: "bug", value: 869003 },
  ], { webconsole: gWebConsole }).then(() => {
    gWebConsole = gJSTerm = gVariablesView = null;
    finishTest();
  });
}
