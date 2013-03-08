/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test" +
                 "/test-bug-766001-js-console-links.html";

let nodes, dbg, toolbox, target, index = 0, src, line;

function test()
{
  expectUncaughtException();
  requestLongerTimeout(2);
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testViewSource);
  }, true);
}

function testViewSource(aHud)
{
  registerCleanupFunction(function() {
    nodes = dbg = toolbox = target = index = src = line = null;
  });

  let JSSelector = ".webconsole-msg-exception .webconsole-location";
  let consoleSelector = ".webconsole-msg-console .webconsole-location";

  waitForSuccess({
    name: "find the location node",
    validatorFn: function()
    {
      return aHud.outputNode.querySelector(JSSelector) &&
             aHud.outputNode.querySelector(consoleSelector);
    },
    successFn: function()
    {
      nodes = [aHud.outputNode.querySelector(JSSelector),
               aHud.outputNode.querySelector(consoleSelector)];

      target = TargetFactory.forTab(gBrowser.selectedTab);
      toolbox = gDevTools.getToolbox(target);
      toolbox.once("jsdebugger-selected", checkLineAndClickNext);

      EventUtils.sendMouseEvent({ type: "click" }, nodes[index%2]);
    },
    failureFn: finishTest,
  });
}

function checkLineAndClickNext(aEvent, aPanel)
{
  if (index == 3) {
    finishTest();
    return;
  }
  info(aEvent + " event fired for index " + index);

  dbg = aPanel;

  src = nodes[index%2].getAttribute("title");
  ok(src, "source url found for index " + index);
  line = nodes[index%2].sourceLine;
  ok(line, "found source line for index " + index);

  info("Waiting for the correct script to be selected for index " + index);
  dbg.panelWin.addEventListener("Debugger:SourceShown", onSource, false);
}

function onSource(aEvent) {
  if (aEvent.detail.url != src) {
    return;
  }
  dbg.panelWin.removeEventListener("Debugger:SourceShown", onSource, false);

  ok(true, "Correct script is selected for index " + index);

  checkCorrectLine(function() {
    gDevTools.showToolbox(target, "webconsole").then(function() {
      index++;
      info("webconsole selected for index " + index);

      toolbox.once("jsdebugger-selected", checkLineAndClickNext);

      EventUtils.sendMouseEvent({ type: "click" }, nodes[index%2]);
    });
  });
}

function checkCorrectLine(aCallback)
{
  waitForSuccess({
    name: "correct source and line test for debugger for index " + index,
    validatorFn: function()
    {
      let debuggerView = dbg.panelWin.DebuggerView;
      if (debuggerView.editor &&
          debuggerView.editor.getCaretPosition().line == line - 1) {
        return true;
      }
      return false;
    },
    successFn: function()
    {
      aCallback && executeSoon(aCallback);
    },
    failureFn: finishTest,
    timeout: 10000,
  });
}
