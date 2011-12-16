/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that console.group/groupEnd works as intended.
const GROUP_INDENT = 12;

function test() {
  addTab("data:text/html,Web Console test for bug 664131: Expand console " +
         "object with group methods");
  browser.addEventListener("load", onLoad, true);
}

function onLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  openConsole();
  let hudId = HUDService.getHudIdByWindow(content);
  let hud = HUDService.hudReferences[hudId];
  outputNode = hud.outputNode;

  content.console.group("a");
  findLogEntry("a");
  let msg = outputNode.querySelectorAll(".webconsole-msg-icon-container");
  is(msg.length, 1, "one message node displayed");
  is(msg[0].style.marginLeft, GROUP_INDENT + "px", "correct group indent found");
  content.console.log("inside");
  findLogEntry("inside");
  let msg = outputNode.querySelectorAll(".webconsole-msg-icon-container");
  is(msg.length, 2, "two message nodes displayed");
  is(msg[1].style.marginLeft, GROUP_INDENT + "px", "correct group indent found");
  content.console.groupEnd("a");
  content.console.log("outside");
  findLogEntry("outside");
  let msg = outputNode.querySelectorAll(".webconsole-msg-icon-container");
  is(msg.length, 3, "three message nodes displayed");
  is(msg[2].style.marginLeft, "0px", "correct group indent found");
  content.console.groupCollapsed("b");
  findLogEntry("b");
  let msg = outputNode.querySelectorAll(".webconsole-msg-icon-container");
  is(msg.length, 4, "four message nodes displayed");
  is(msg[3].style.marginLeft, GROUP_INDENT + "px", "correct group indent found");

  // Test that clearing the console removes the indentation.
  hud.jsterm.clearOutput();
  content.console.log("cleared");
  findLogEntry("cleared");
  let msg = outputNode.querySelectorAll(".webconsole-msg-icon-container");
  is(msg.length, 1, "one message node displayed");
  is(msg[0].style.marginLeft, "0px", "correct group indent found");

  finishTest();
}

