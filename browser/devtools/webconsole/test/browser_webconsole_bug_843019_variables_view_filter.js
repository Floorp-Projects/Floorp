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
  hud.jsterm.execute("fooObj", (msg) => {
    ok(msg, "output message found");
    isnot(msg.textContent.indexOf("[object Object]"), -1, "message text check");

    hud.jsterm.once("variablesview-fetched", (aEvent, aVar) => {
      gVariablesView = aVar._variablesView;
      ok(gVariablesView, "variables view object");

      findVariableViewProperties(aVar, [
        { name: "testProp", value: "testValue" },
      ], { webconsole: hud }).then(onTestPropFound);
    });

    let anchor = msg.querySelector("a");
    executeSoon(() =>
      EventUtils.synthesizeMouse(anchor, 2, 2, {}, hud.iframeWindow)
    );
  });
}

let console = Cu.import("resource://gre/modules/devtools/Console.jsm", {}).console;

function onTestPropFound([result])
{
  let target = result.matchedProp.target;
  let searchbox = gVariablesView._searchboxContainer.firstChild;
  gVariablesView.lazySearch = false;

  searchbox.addEventListener("focus", function onFocus() {
    searchbox.removeEventListener("focus", onFocus);

    // Test initial state.
    ok(!target.hasAttribute("non-match"),
      "Property starts visible");

    // Test a non-matching search.
    EventUtils.sendChar("x");
    ok(target.hasAttribute("non-match"),
      "Property is hidden on non-matching search");

    // Test clearing the search.
    EventUtils.sendKey("ESCAPE");
    ok(!target.hasAttribute("non-match"),
      "Pressing ESC makes the property visible again");

    // Test a matching search.
    EventUtils.sendChar("t");
    ok(!target.hasAttribute("non-match"),
      "Property still visible when search matches");

    gVariablesView = null;
    finishTest();
  });

  searchbox.focus();
}
