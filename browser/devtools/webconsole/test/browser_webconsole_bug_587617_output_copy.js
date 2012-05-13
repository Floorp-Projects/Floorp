/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(HUD) {
  // See bugs 574036, 586386 and 587617.
  outputNode = HUD.outputNode;
  let selection = getSelection();
  let jstermInput = HUD.jsterm.inputNode;
  let console = content.wrappedJSObject.console;
  let contentSelection = content.wrappedJSObject.getSelection();

  HUD.jsterm.clearOutput();

  let controller = top.document.commandDispatcher.
                   getControllerForCommand("cmd_copy");
  is(controller.isCommandEnabled("cmd_copy"), false, "cmd_copy is disabled");

  console.log("Hello world! bug587617");

  waitForSuccess({
    name: "console log 'Hello world!' message",
    validatorFn: function()
    {
      return outputNode.textContent.indexOf("bug587617") > -1;
    },
    successFn: function()
    {
      outputNode.selectedIndex = 0;
      outputNode.focus();

      goUpdateCommand("cmd_copy");
      controller = top.document.commandDispatcher.
        getControllerForCommand("cmd_copy");
      is(controller.isCommandEnabled("cmd_copy"), true, "cmd_copy is enabled");
      let selectedNode = outputNode.getItemAtIndex(0);
      waitForClipboard(getExpectedClipboardText(selectedNode), clipboardSetup,
                       testContextMenuCopy, testContextMenuCopy);
    },
    failureFn: finishTest,
  });
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
  return "[" + WebConsoleUtils.l10n.timestampString(aItem.timestamp) + "] " +
         aItem.clipboardText;
}

function clipboardSetup() {
  goDoCommand("cmd_copy");
}

