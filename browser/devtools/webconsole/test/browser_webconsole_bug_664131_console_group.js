/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that console.group/groupEnd works as intended.
const GROUP_INDENT = 12;

let testDriver, hud;

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 664131: Expand console " +
         "object with group methods");
  browser.addEventListener("load", function onLoad(aEvent) {
    browser.removeEventListener(aEvent.type, onLoad, true);
    openConsole(null, function(aHud) {
      hud = aHud;
      testDriver = testGen();
      testNext();
    });
  }, true);
}

function testNext() {
  testDriver.next();
}

function testGen() {
  outputNode = hud.outputNode;

  hud.jsterm.clearOutput();

  content.console.group("bug664131a");

  waitForSuccess({
    name: "console.group displayed",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("bug664131a") > -1;
    },
    successFn: testNext,
    failureFn: finishTest,
  });

  yield;

  let msg = outputNode.querySelectorAll(".webconsole-msg-icon-container");
  is(msg.length, 1, "one message node displayed");
  is(msg[0].style.marginLeft, GROUP_INDENT + "px", "correct group indent found");

  content.console.log("bug664131a-inside");

  waitForSuccess({
    name: "console.log message displayed",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("bug664131a-inside") > -1;
    },
    successFn: testNext,
    failureFn: finishTest,
  });

  yield;

  msg = outputNode.querySelectorAll(".webconsole-msg-icon-container");
  is(msg.length, 2, "two message nodes displayed");
  is(msg[1].style.marginLeft, GROUP_INDENT + "px", "correct group indent found");

  content.console.groupEnd("bug664131a");
  content.console.log("bug664131-outside");

  waitForSuccess({
    name: "console.log message displayed after groupEnd()",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("bug664131-outside") > -1;
    },
    successFn: testNext,
    failureFn: finishTest,
  });

  yield;

  msg = outputNode.querySelectorAll(".webconsole-msg-icon-container");
  is(msg.length, 3, "three message nodes displayed");
  is(msg[2].style.marginLeft, "0px", "correct group indent found");

  content.console.groupCollapsed("bug664131b");

  waitForSuccess({
    name: "console.groupCollapsed displayed",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("bug664131b") > -1;
    },
    successFn: testNext,
    failureFn: finishTest,
  });

  yield;

  msg = outputNode.querySelectorAll(".webconsole-msg-icon-container");
  is(msg.length, 4, "four message nodes displayed");
  is(msg[3].style.marginLeft, GROUP_INDENT + "px", "correct group indent found");


  // Test that clearing the console removes the indentation.
  hud.jsterm.clearOutput();
  content.console.log("bug664131-cleared");

  waitForSuccess({
    name: "console.log displayed after clearOutput",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("bug664131-cleared") > -1;
    },
    successFn: testNext,
    failureFn: finishTest,
  });

  yield;

  msg = outputNode.querySelectorAll(".webconsole-msg-icon-container");
  is(msg.length, 1, "one message node displayed");
  is(msg[0].style.marginLeft, "0px", "correct group indent found");

  testDriver = hud = null;
  finishTest();

  yield;
}

