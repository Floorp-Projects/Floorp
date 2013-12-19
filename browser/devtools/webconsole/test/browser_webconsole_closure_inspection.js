/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that inspecting a closure in the variables view sidebar works when
// execution is paused.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-closures.html";

let gWebConsole, gJSTerm, gVariablesView;

function test()
{
  registerCleanupFunction(() => {
    gWebConsole = gJSTerm = gVariablesView = null;
  });

  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, (hud) => {
      openDebugger().then(({ toolbox, panelWin }) => {
        let deferred = promise.defer();
        panelWin.gThreadClient.addOneTimeListener("resumed", (aEvent, aPacket) => {
          ok(true, "Debugger resumed");
          deferred.resolve({ toolbox: toolbox, panelWin: panelWin });
        });
        return deferred.promise;
      }).then(({ toolbox, panelWin }) => {
        let deferred = promise.defer();
        panelWin.once(panelWin.EVENTS.FETCHED_SCOPES, (aEvent, aPacket) => {
          ok(true, "Scopes were fetched");
          toolbox.selectTool("webconsole").then(() => consoleOpened(hud));
          deferred.resolve();
        });

        let button = content.document.querySelector("button");
        ok(button, "button element found");
        button.click();

        return deferred.promise;
      });
    });
  }, true);
}

function consoleOpened(hud)
{
  gWebConsole = hud;
  gJSTerm = hud.jsterm;
  gJSTerm.execute("window.george.getName");

  waitForMessages({
    webconsole: gWebConsole,
    messages: [{
      text: "function _pfactory/<.getName()",
      category: CATEGORY_OUTPUT,
      objects: true,
    }],
  }).then(onExecuteGetName);
}

function onExecuteGetName(aResults)
{
  let clickable = aResults[0].clickableElements[0];
  ok(clickable, "clickable object found");

  gJSTerm.once("variablesview-fetched", onGetNameFetch);
  EventUtils.synthesizeMouse(clickable, 2, 2, {}, gWebConsole.iframeWindow)
}

function onGetNameFetch(aEvent, aVar)
{
  gVariablesView = aVar._variablesView;
  ok(gVariablesView, "variables view object");

  findVariableViewProperties(aVar, [
    { name: /_pfactory/, value: "" },
  ], { webconsole: gWebConsole }).then(onExpandClosure);
}

function onExpandClosure(aResults)
{
  let prop = aResults[0].matchedProp;
  ok(prop, "matched the name property in the variables view");

  gVariablesView.window.focus();
  gJSTerm.once("sidebar-closed", finishTest);
  EventUtils.synthesizeKey("VK_ESCAPE", {}, gVariablesView.window);
}
