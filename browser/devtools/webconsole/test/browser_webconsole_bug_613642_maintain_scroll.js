/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Șucan <mihai.sucan@gmail.com>
 */

let hud, testDriver;

function testNext() {
  testDriver.next();
}

function testGen() {
  hud.jsterm.clearOutput();
  let outputNode = hud.outputNode;
  let scrollBox = outputNode.scrollBoxObject.element;

  for (let i = 0; i < 150; i++) {
    hud.console.log("test message " + i);
  }

  waitForSuccess({
    name: "150 console.log messages displayed",
    validatorFn: function()
    {
      return outputNode.querySelectorAll(".hud-log").length == 150;
    },
    successFn: testNext,
    failureFn: finishTest,
  });

  yield;

  let oldScrollTop = scrollBox.scrollTop;
  ok(oldScrollTop > 0, "scroll location is not at the top");

  // scroll to the first node
  outputNode.focus();

  EventUtils.synthesizeKey("VK_HOME", {});

  let topPosition = scrollBox.scrollTop;
  isnot(topPosition, oldScrollTop, "scroll location updated (moved to top)");

  // add a message and make sure scroll doesn't change
  hud.console.log("test message 150");

  waitForSuccess({
    name: "console.log message no. 151 displayed",
    validatorFn: function()
    {
      return outputNode.querySelectorAll(".hud-log").length == 151;
    },
    successFn: testNext,
    failureFn: finishTest,
  });

  yield;

  is(scrollBox.scrollTop, topPosition, "scroll location is still at the top");

  // scroll back to the bottom
  outputNode.lastChild.focus();
  EventUtils.synthesizeKey("VK_END", {});

  oldScrollTop = outputNode.scrollTop;

  hud.console.log("test message 151");

  waitForSuccess({
    name: "console.log message no. 152 displayed",
    validatorFn: function()
    {
      return outputNode.querySelectorAll(".hud-log").length == 152;
    },
    successFn: testNext,
    failureFn: finishTest,
  });

  yield;

  isnot(scrollBox.scrollTop, oldScrollTop,
        "scroll location updated (moved to bottom)");

  hud = testDriver = null;
  finishTest();
  
  yield;
}

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 613642: remember scroll location");
  browser.addEventListener("load", function tabLoad(aEvent) {
    browser.removeEventListener(aEvent.type, tabLoad, true);
    openConsole(null, function(aHud) {
      hud = aHud;
      testDriver = testGen();
      testDriver.next();
    });
  }, true);
}
