/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Reference to the Scratchpad chrome window object.
let gScratchpadWindow;

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    gScratchpadWindow = Scratchpad.openScratchpad();
    gScratchpadWindow.addEventListener("load", runTests, false);
  }, true);

  content.location = "data:text/html,<title>foobarBug636725</title>" +
    "<p>test inspect() in Scratchpad";
}

function runTests()
{
  gScratchpadWindow.removeEventListener("load", arguments.callee, false);

  let sp = gScratchpadWindow.Scratchpad;
  let doc = gScratchpadWindow.document;

  let methodsAndItems = {
    "sp-menu-newscratchpad": "openScratchpad",
    "sp-menu-open": "openFile",
    "sp-menu-save": "saveFile",
    "sp-menu-saveas": "saveFileAs",
    "sp-text-run": "run",
    "sp-text-inspect": "inspect",
    "sp-text-display": "display",
    "sp-menu-content": "setContentContext",
    "sp-menu-browser": "setBrowserContext",
    "sp-menu-resetContext": "resetContext",
    "sp-menu-errorConsole": "openErrorConsole",
    "sp-menu-webConsole": "openWebConsole",
    "sp-menu-undo": "undo",
    "sp-menu-redo": "redo",
  };

  let lastMethodCalled = null;
  sp.__noSuchMethod__ = function(aMethodName) {
    lastMethodCalled = aMethodName;
  };

  for (let id in methodsAndItems) {
    lastMethodCalled = null;

    let methodName = methodsAndItems[id];
    let oldMethod = sp[methodName];
    ok(oldMethod, "found method " + methodName + " in Scratchpad object");

    delete sp[methodName];

    let menu = doc.getElementById(id);
    ok(menu, "found menuitem #" + id);

    try {
      menu.doCommand();
    }
    catch (ex) {
      ok(false, "exception thrown while executing the command of menuitem #" + id);
    }

    ok(lastMethodCalled == methodName,
       "method " + methodName + " invoked by the associated menuitem");

    sp[methodName] = oldMethod;
  }

  delete sp.__noSuchMethod__;

  gScratchpadWindow.close();
  gScratchpadWindow = null;
  gBrowser.removeCurrentTab();
  finish();
}
