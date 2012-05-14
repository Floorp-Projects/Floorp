/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded",
                           testSelectionWhenMovingBetweenBoxes, false);
}

function testSelectionWhenMovingBetweenBoxes() {
  browser.removeEventListener("DOMContentLoaded",
                              testSelectionWhenMovingBetweenBoxes, false);
  openConsole();

  let jsterm = HUDService.getHudByWindow(content).jsterm;

  // Fill the console with some output.
  jsterm.clearOutput();
  jsterm.execute("1 + 2");
  jsterm.execute("3 + 4");
  jsterm.execute("5 + 6");

  outputNode = jsterm.outputNode;

  ok(outputNode.childNodes.length >= 3, "the output node has children after " +
     "executing some JavaScript");

  // Test that the global Firefox "Select All" functionality (e.g. Edit >
  // Select All) works properly in the Web Console.
  let commandController = window.webConsoleCommandController;
  ok(commandController != null, "the window has a command controller object");

  commandController.selectAll(outputNode);
  is(outputNode.selectedCount, outputNode.childNodes.length, "all console " +
     "messages are selected after performing a regular browser select-all " +
     "operation");

  outputNode.selectedIndex = -1;

  // Test the context menu "Select All" (which has a different code path) works
  // properly as well.
  let contextMenuId = outputNode.getAttribute("context");
  let contextMenu = document.getElementById(contextMenuId);
  ok(contextMenu != null, "the output node has a context menu");

  let selectAllItem = contextMenu.querySelector("*[buttonType=\"selectAll\"]");
  ok(selectAllItem != null,
     "the context menu on the output node has a \"Select All\" item");

  let commandEvent = document.createEvent("XULCommandEvent");
  commandEvent.initCommandEvent("command", true, true, window, 0, false, false,
                                false, false, null);
  selectAllItem.dispatchEvent(commandEvent);

  is(outputNode.selectedCount, outputNode.childNodes.length, "all console " +
     "messages are selected after performing a select-all operation from " +
     "the context menu");

  outputNode.selectedIndex = -1;

  finishTest();
}

