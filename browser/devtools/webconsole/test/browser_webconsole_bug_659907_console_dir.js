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
  outputNode = hud.outputNode;
  content.console.dir(content.document);
  waitForSuccess({
    name: "console.dir displayed",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("[object HTMLDocument") > -1;
    },
    successFn: testConsoleDir.bind(null, outputNode),
    failureFn: finishTest,
  });
}

function testConsoleDir(outputNode) {
  let msg = outputNode.querySelectorAll(".webconsole-msg-inspector");
  is(msg.length, 1, "one message node displayed");
  let view = msg[0].propertyTreeView;
  let foundQSA = false;
  let foundLocation = false;
  let foundWrite = false;
  for (let i = 0; i < view.rowCount; i++) {
    let text = view.getCellText(i);
    if (text == "querySelectorAll: function querySelectorAll()") {
      foundQSA = true;
    }
    else if (text  == "location: Object") {
      foundLocation = true;
    }
    else if (text  == "write: function write()") {
      foundWrite = true;
    }
  }
  ok(foundQSA, "found document.querySelectorAll");
  ok(foundLocation, "found document.location");
  ok(foundWrite, "found document.write");
  msg = view = outputNode = null;
  executeSoon(finishTest);
}
