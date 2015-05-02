/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that the variables view sidebar can be closed by pressing Escape in the
// web console.

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-eval-in-stackframe.html";

function test()
{
  let hud;

  Task.spawn(runner).then(finishTest);

  function* runner() {
    let {tab} = yield loadTab(TEST_URI);
    hud = yield openConsole(tab);
    let jsterm = hud.jsterm;
    let result;
    let vview;
    let msg;

    yield openSidebar("fooObj",
                      'testProp: "testValue"',
                      { name: "testProp", value: "testValue" });

    let prop = result.matchedProp;
    ok(prop, "matched the |testProp| property in the variables view");

    vview.window.focus();

    let sidebarClosed = jsterm.once("sidebar-closed");
    EventUtils.synthesizeKey("VK_ESCAPE", {});
    yield sidebarClosed;

    jsterm.clearOutput();

    yield openSidebar("window.location",
                      "Location \u2192 http://example.com/browser/",
                      { name: "host", value: "example.com" });

    vview.window.focus();

    msg.scrollIntoView();
    sidebarClosed = jsterm.once("sidebar-closed");
    EventUtils.synthesizeKey("VK_ESCAPE", {});
    yield sidebarClosed;

    function* openSidebar(objName, expectedText, expectedObj) {
      msg = yield jsterm.execute(objName);
      ok(msg, "output message found");

      let anchor = msg.querySelector("a");
      let body = msg.querySelector(".message-body");
      ok(anchor, "object anchor");
      ok(body, "message body");
      ok(body.textContent.includes(expectedText), "message text check");

      msg.scrollIntoView();
      yield EventUtils.synthesizeMouse(anchor, 2, 2, {}, hud.iframeWindow);

      let vviewVar = yield jsterm.once("variablesview-fetched");
      vview = vviewVar._variablesView;
      ok(vview, "variables view object exists");

      [result] = yield findVariableViewProperties(vviewVar, [
        expectedObj,
      ], { webconsole: hud });
    }
  }
}
