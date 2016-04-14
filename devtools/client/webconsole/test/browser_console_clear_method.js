/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that calls to console.clear from a script delete the messages
// previously logged.

"use strict";

add_task(function* () {
  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                   "test/test-console-clear.html";

  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  ok(hud, "Web Console opened");

  info("Check the console.clear() done on page load has been processed.");
  yield waitForLog("Console was cleared", hud);
  ok(hud.outputNode.textContent.includes("Console was cleared"),
    "console.clear() message is displayed");
  ok(!hud.outputNode.textContent.includes("log1"), "log1 not displayed");
  ok(!hud.outputNode.textContent.includes("log2"), "log2 not displayed");

  info("Logging two messages log3, log4");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    content.wrappedJSObject.console.log("log3");
    content.wrappedJSObject.console.log("log4");
  });

  yield waitForLog("log3", hud);
  yield waitForLog("log4", hud);

  ok(hud.outputNode.textContent.includes("Console was cleared"),
    "console.clear() message is still displayed");
  ok(hud.outputNode.textContent.includes("log3"), "log3 is displayed");
  ok(hud.outputNode.textContent.includes("log4"), "log4 is displayed");

  info("Open the variables view sidebar for 'objFromPage'");
  yield openSidebar("objFromPage", { a: 1 }, hud);
  let sidebarClosed = hud.jsterm.once("sidebar-closed");

  info("Call console.clear from the page");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    content.wrappedJSObject.console.clear();
  });

  // Cannot wait for "Console was cleared" here because such a message is
  // already present and would yield immediately.
  info("Wait for variables view sidebar to be closed after console.clear()");
  yield sidebarClosed;

  ok(!hud.outputNode.textContent.includes("log3"), "log3 not displayed");
  ok(!hud.outputNode.textContent.includes("log4"), "log4 not displayed");
  ok(hud.outputNode.textContent.includes("Console was cleared"),
    "console.clear() message is still displayed");
  is(hud.outputNode.textContent.split("Console was cleared").length, 2,
    "console.clear() message is only displayed once");

  info("Logging one messages log5");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    content.wrappedJSObject.console.log("log5");
  });
  yield waitForLog("log5", hud);

  info("Close and reopen the webconsole.");
  yield closeConsole(gBrowser.selectedTab);
  hud = yield openConsole();
  yield waitForLog("Console was cleared", hud);

  ok(hud.outputNode.textContent.includes("Console was cleared"),
    "console.clear() message is still displayed");
  ok(!hud.outputNode.textContent.includes("log1"), "log1 not displayed");
  ok(!hud.outputNode.textContent.includes("log2"), "log1 not displayed");
  ok(!hud.outputNode.textContent.includes("log3"), "log3 not displayed");
  ok(!hud.outputNode.textContent.includes("log4"), "log4 not displayed");
  ok(hud.outputNode.textContent.includes("log5"), "log5 still displayed");
});

/**
 * Wait for a single message to be logged in the provided webconsole instance
 * with the category CATEGORY_WEBDEV and the SEVERITY_LOG severity.
 *
 * @param {String} message
 *        The expected messaged.
 * @param {WebConsole} webconsole
 *        WebConsole instance in which the message should be logged.
 */
function* waitForLog(message, webconsole, options) {
  yield waitForMessages({
    webconsole: webconsole,
    messages: [{
      text: message,
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });
}

/**
 * Open the variables view sidebar for the object with the provided name objName
 * and wait for the expected object is displayed in the variables view.
 *
 * @param {String} objName
 *        The name of the object to open in the sidebar.
 * @param {Object} expectedObj
 *        The properties that should be displayed in the variables view.
 * @param {WebConsole} webconsole
 *        WebConsole instance in which the message should be logged.
 *
 */
function* openSidebar(objName, expectedObj, webconsole) {
  let msg = yield webconsole.jsterm.execute(objName);
  ok(msg, "output message found");

  let anchor = msg.querySelector("a");
  let body = msg.querySelector(".message-body");
  ok(anchor, "object anchor");
  ok(body, "message body");

  yield EventUtils.synthesizeMouse(anchor, 2, 2, {}, webconsole.iframeWindow);

  let vviewVar = yield webconsole.jsterm.once("variablesview-fetched");
  let vview = vviewVar._variablesView;
  ok(vview, "variables view object exists");

  yield findVariableViewProperties(vviewVar, [
    expectedObj,
  ], { webconsole: webconsole });
}
