/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Șucan <mihai.sucan@gmail.com>
 */

function consoleOpened(hud) {
  hud.jsterm.clearOutput();

  let outputNode = hud.outputNode;
  let boxObject = outputNode.scrollBoxObject.element;

  for (let i = 0; i < 150; i++) {
    content.console.log("test message " + i);
  }

  let oldScrollTop = -1;

  waitForSuccess({
    name: "console.log messages displayed",
    validatorFn: function()
    {
      return outputNode.itemCount == 150;
    },
    successFn: function()
    {
      oldScrollTop = boxObject.scrollTop;
      ok(oldScrollTop > 0, "scroll location is not at the top");

      hud.jsterm.execute("'hello world'");

      waitForSuccess(waitForExecute);
    },
    failureFn: finishTest,
  });

  let waitForExecute = {
    name: "jsterm output displayed",
    validatorFn: function()
    {
      return outputNode.querySelector(".webconsole-msg-output");
    },
    successFn: function()
    {
      isnot(boxObject.scrollTop, oldScrollTop, "scroll location updated");

      oldScrollTop = boxObject.scrollTop;
      outputNode.scrollBoxObject.ensureElementIsVisible(outputNode.lastChild);

      is(boxObject.scrollTop, oldScrollTop, "scroll location is the same");

      finishTest();
    },
    failureFn: finishTest,
  };
}

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 614793: jsterm result scroll");
  browser.addEventListener("load", function onLoad(aEvent) {
    browser.removeEventListener(aEvent.type, onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

