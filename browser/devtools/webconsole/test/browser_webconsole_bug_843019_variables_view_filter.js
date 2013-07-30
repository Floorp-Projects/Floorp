/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test for bug 843019.
// Check that variables view filter works as expected in the web console.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-eval-in-stackframe.html";

let gVariablesView;

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
  hud.jsterm.execute("fooObj", () => {
    let msg = hud.outputNode.querySelector(".webconsole-msg-output");
    ok(msg, "output message found");
    isnot(msg.textContent.indexOf("[object Object]"), -1, "message text check");

    hud.jsterm.once("variablesview-fetched", (aEvent, aVar) => {
      gVariablesView = aVar._variablesView;
      ok(gVariablesView, "variables view object");

      findVariableViewProperties(aVar, [
        { name: "testProp", value: "testValue" },
      ], { webconsole: hud }).then(onTestPropFound);
    });

    executeSoon(() =>
      EventUtils.synthesizeMouse(msg, 2, 2, {}, hud.iframeWindow)
    );
  });
}

function onTestPropFound([result])
{
  let target = result.matchedProp.target;
  let win = gVariablesView.window;
  win.focus();
  gVariablesView._searchboxContainer.firstChild.focus();

  ok(!target.hasAttribute("non-match"),
    "Property starts visible");
  EventUtils.synthesizeKey("X", {}, win);
  ok(target.hasAttribute("non-match"),
    "Property is hidden on non-matching search");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  ok(!target.hasAttribute("non-match"),
    "Pressing ESC makes the property visible again");
  EventUtils.synthesizeKey("t", {}, win);
  ok(!target.hasAttribute("non-match"),
    "Property still visible when search matches");

  gVariablesView = null;
  finishTest();
}
