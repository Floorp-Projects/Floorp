/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that the variables view sidebar can be closed by pressing Escape in the
// web console.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-eval-in-stackframe.html";

function test()
{
  let hud;

  Task.spawn(runner).then(finishTest);

  function* runner() {
    let {tab} = yield loadTab(TEST_URI);
    hud = yield openConsole(tab);
    let jsterm = hud.jsterm;

    let msg = yield execute("fooObj");
    ok(msg, "output message found");

    let anchor = msg.querySelector("a");
    let body = msg.querySelector(".message-body");
    ok(anchor, "object anchor");
    ok(body, "message body");
    ok(body.textContent.contains('testProp: "testValue"'), "message text check");

    msg.scrollIntoView();
    executeSoon(() => {
      EventUtils.synthesizeMouse(anchor, 2, 2, {}, hud.iframeWindow);
    });

    let vviewVar = yield jsterm.once("variablesview-fetched");
    let vview = vviewVar._variablesView;
    ok(vview, "variables view object");

    let [result] = yield findVariableViewProperties(vviewVar, [
      { name: "testProp", value: "testValue" },
    ], { webconsole: hud });

    let prop = result.matchedProp;
    ok(prop, "matched the |testProp| property in the variables view");

    is(content.wrappedJSObject.fooObj.testProp, result.value,
       "|fooObj.testProp| value is correct");

    vview.window.focus();

    executeSoon(() => {
      EventUtils.synthesizeKey("VK_ESCAPE", {});
    });
    yield jsterm.once("sidebar-closed");

    jsterm.clearOutput();

    msg = yield execute("window.location");
    ok(msg, "output message found");

    body = msg.querySelector(".message-body");
    ok(body, "message body");
    anchor = msg.querySelector("a");
    ok(anchor, "object anchor");
    ok(body.textContent.contains("Location \u2192 http://example.com/browser/"),
       "message text check");

    msg.scrollIntoView();
    executeSoon(() => {
      EventUtils.synthesizeMouse(anchor, 2, 2, {}, hud.iframeWindow)
    });
    vviewVar = yield jsterm.once("variablesview-fetched");

    vview = vviewVar._variablesView;
    ok(vview, "variables view object");

    yield findVariableViewProperties(vviewVar, [
      { name: "host", value: "example.com" },
    ], { webconsole: hud });

    vview.window.focus();

    msg.scrollIntoView();
    executeSoon(() => {
      EventUtils.synthesizeKey("VK_ESCAPE", {});
    });

    yield jsterm.once("sidebar-closed");
  }

  function execute(str) {
    let deferred = promise.defer();
    hud.jsterm.execute(str, (msg) => {
      deferred.resolve(msg);
    });
    return deferred.promise;
  }
}
