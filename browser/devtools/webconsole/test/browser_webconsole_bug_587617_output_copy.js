/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test//test-console.html";

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test() {
  Services.prefs.setBoolPref("devtools.gcli.enable", false);
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded() {
  browser.removeEventListener("load", tabLoaded, true);
  openConsole();

  // See bugs 574036, 586386 and 587617.

  let HUD = HUDService.getHudByWindow(content);
  outputNode = HUD.outputNode;
  let selection = getSelection();
  let jstermInput = HUD.jsterm.inputNode;
  let console = content.wrappedJSObject.console;
  let contentSelection = content.wrappedJSObject.getSelection();

  let make_selection = function () {
    let controller =
      top.document.commandDispatcher.
      getControllerForCommand("cmd_copy");
    is(controller.isCommandEnabled("cmd_copy"), false, "cmd_copy is disabled");

    console.log("Hello world!");

    outputNode.selectedIndex = 0;
    outputNode.focus();

    goUpdateCommand("cmd_copy");

    controller = top.document.commandDispatcher.
      getControllerForCommand("cmd_copy");
    is(controller.isCommandEnabled("cmd_copy"), true, "cmd_copy is enabled");

    let selectedNode = outputNode.getItemAtIndex(0);
    waitForClipboard(getExpectedClipboardText(selectedNode), clipboardSetup,
                     testContextMenuCopy, testContextMenuCopy);
  };

  make_selection();
}

// Test that the context menu "Copy" (which has a different code path) works
// properly as well.
function testContextMenuCopy() {
  let contextMenuId = outputNode.getAttribute("context");
  let contextMenu = document.getElementById(contextMenuId);
  ok(contextMenu, "the output node has a context menu");

  let copyItem = contextMenu.querySelector("*[buttonType=\"copy\"]");
  ok(copyItem, "the context menu on the output node has a \"Copy\" item");

  let commandEvent = document.createEvent("XULCommandEvent");
  commandEvent.initCommandEvent("command", true, true, window, 0, false, false,
                                false, false, null);
  copyItem.dispatchEvent(commandEvent);

  let selectedNode = outputNode.getItemAtIndex(0);
  waitForClipboard(getExpectedClipboardText(selectedNode), clipboardSetup,
    finishTest, finishTest);
}

function getExpectedClipboardText(aItem) {
  return "[" + ConsoleUtils.timestampString(aItem.timestamp) + "] " +
         aItem.clipboardText;
}

function clipboardSetup() {
  goDoCommand("cmd_copy");
}

