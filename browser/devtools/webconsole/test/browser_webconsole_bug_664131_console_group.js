/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that console.group/groupEnd works as intended.

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

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bug664131a",
      consoleGroup: 1,
    }],
  }).then(testNext);

  yield undefined;

  content.console.log("bug664131a-inside");

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bug664131a-inside",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      groupDepth: 1,
    }],
  }).then(testNext);

  yield undefined;

  content.console.groupEnd("bug664131a");
  content.console.log("bug664131-outside");

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bug664131-outside",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      groupDepth: 0,
    }],
  }).then(testNext);

  yield undefined;

  content.console.groupCollapsed("bug664131b");

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bug664131b",
      consoleGroup: 1,
    }],
  }).then(testNext);

  yield undefined;

  // Test that clearing the console removes the indentation.
  hud.jsterm.clearOutput();
  content.console.log("bug664131-cleared");

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "bug664131-cleared",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      groupDepth: 0,
    }],
  }).then(testNext);

  yield undefined;

  testDriver = hud = null;
  finishTest();

  yield undefined;
}

