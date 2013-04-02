/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that makes sure messages are not considered repeated when coming from
// different lines of code, or from different severities, etc.
// See bugs 720180 and 800510.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-repeated-messages.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(hud) {
  // Check that css warnings are not coalesced if they come from different lines.
  waitForSuccess({
    name: "css warnings displayed",
    validatorFn: function()
    {
      return hud.outputNode.querySelectorAll(".webconsole-msg-cssparser")
             .length == 2;
    },
    successFn: testCSSRepeats.bind(null, hud),
    failureFn: finishTest,
  });
}

function repeatCountForNode(aNode) {
  return aNode.querySelector(".webconsole-msg-repeat").getAttribute("value");
}

function testCSSRepeats(hud) {
  let msgs = hud.outputNode.querySelectorAll(".webconsole-msg-cssparser");
  is(repeatCountForNode(msgs[0]), 1, "no repeats for the first css warning");
  is(repeatCountForNode(msgs[1]), 1, "no repeats for the second css warning");

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    testAfterReload(hud);
  }, true);
  content.location.reload();
}

function testAfterReload(hud) {
  let repeats;
  waitForSuccess({
    name: "message repeats increased",
    validatorFn: () => {
      repeats = hud.outputNode.querySelectorAll(".webconsole-msg-cssparser " +
                                                ".webconsole-msg-repeat");
      return repeats.length == 2 &&
             repeats[0].getAttribute("value") == 2 &&
             repeats[1].getAttribute("value") == 2;
    },
    successFn: testCSSRepeatsAfterReload.bind(null, hud),
    failureFn: () => {
      let repeats0 = repeats[0] ? repeats[0].getAttribute("value") : "undefined";
      let repeats1 = repeats[1] ? repeats[1].getAttribute("value") : "undefined";
      info("repeats.length " + repeats.length);
      info("repeats[0] value " + repeats0);
      info("repeats[1] value " + repeats1);
      finishTest();
    },
  });
}

function testCSSRepeatsAfterReload(hud) {
  let msgs = hud.outputNode.querySelectorAll(".webconsole-msg-cssparser");
  is(msgs.length, 2, "two css warnings after reload");
  is(repeatCountForNode(msgs[0]), 2, "two repeats for the first css warning");
  is(repeatCountForNode(msgs[1]), 2, "two repeats for the second css warning");

  hud.jsterm.clearOutput();
  content.wrappedJSObject.testConsole();

  waitForSuccess({
    name: "console API messages displayed",
    validatorFn: function()
    {
      return hud.outputNode.querySelectorAll(".webconsole-msg-console")
             .length == 3;
    },
    successFn: testConsoleRepeats.bind(null, hud),
    failureFn: finishTest,
  });
}

function testConsoleRepeats(hud) {
  let msgs = hud.outputNode.querySelectorAll(".webconsole-msg-console");
  is(repeatCountForNode(msgs[0]), 2, "repeats for the first console message");
  is(repeatCountForNode(msgs[1]), 1,
     "no repeats for the second console log message");
  is(repeatCountForNode(msgs[2]), 1, "no repeats for the console.error message");

  hud.jsterm.clearOutput();
  hud.jsterm.execute("undefined");
  content.console.log("undefined");

  waitForSuccess({
    name: "messages displayed",
    validatorFn: function()
    {
      return hud.outputNode.querySelector(".webconsole-msg-console");
    },
    successFn: function() {
      is(hud.outputNode.childNodes.length, 3,
         "correct number of messages displayed");
      executeSoon(finishTest);
    },
    failureFn: finishTest,
  });
}
