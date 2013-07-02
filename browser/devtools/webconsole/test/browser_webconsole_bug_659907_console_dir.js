/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that console.dir works as intended.

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 659907: Expand console " +
         "object with a dir method");
  browser.addEventListener("load", function onLoad(aEvent) {
    browser.removeEventListener(aEvent.type, onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(hud) {
  hud.jsterm.execute("console.dir(document)");
  hud.jsterm.once("variablesview-fetched", testConsoleDir.bind(null, hud));
}

function testConsoleDir(hud, ev, view) {
  findVariableViewProperties(view, [
    { name: "__proto__.__proto__.querySelectorAll", value: "Function" },
    { name: "location", value: "Location" },
    { name: "__proto__.write", value: "Function" },
  ], { webconsole: hud }).then(finishTest);
}
