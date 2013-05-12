/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that the output for console.dir() works even if Logging filter is off.

const TEST_URI = "data:text/html;charset=utf8,<p>test for bug 862916";

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
  ok(hud, "web console opened");

  hud.setFilterState("log", false);
  registerCleanupFunction(() => hud.setFilterState("log", true));

  content.wrappedJSObject.fooBarz = "bug862916";
  hud.jsterm.execute("console.dir(window)");
  hud.jsterm.once("variablesview-fetched", (aEvent, aVar) => {
    ok(aVar, "variables view object");
    findVariableViewProperties(aVar, [
      { name: "fooBarz", value: "bug862916" },
    ], { webconsole: hud }).then(finishTest);
  });
}
