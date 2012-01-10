/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Șucan <mihai.sucan@gmail.com>
 */

const TEST_URI = "data:text/html,Web Console test for bug 613280";

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

  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];
  let input = HUD.jsterm.inputNode;
  let selection = getSelection();
  let contentSelection = browser.contentWindow.wrappedJSObject.getSelection();

  let clipboard_setup = function() {
    goDoCommand("cmd_copy");
  };

  let clipboard_copy_done = function() {
    finishTest();
  };

  // Check if we first need to clear any existing selections.
  if (selection.rangeCount > 0 || contentSelection.rangeCount > 0 ||
      input.selectionStart != input.selectionEnd) {
    if (input.selectionStart != input.selectionEnd) {
      input.selectionStart = input.selectionEnd = 0;
    }

    if (selection.rangeCount > 0) {
      selection.removeAllRanges();
    }

    if (contentSelection.rangeCount > 0) {
      contentSelection.removeAllRanges();
    }

    goUpdateCommand("cmd_copy");
  }

  let controller = top.document.commandDispatcher.
                   getControllerForCommand("cmd_copy");
  is(controller.isCommandEnabled("cmd_copy"), false, "cmd_copy is disabled");

  HUD.jsterm.execute("'bug613280: hello world!'");

  HUD.outputNode.selectedIndex = HUD.outputNode.itemCount - 1;
  HUD.outputNode.focus();

  goUpdateCommand("cmd_copy");

  controller = top.document.commandDispatcher.
               getControllerForCommand("cmd_copy");
  is(controller.isCommandEnabled("cmd_copy"), true, "cmd_copy is enabled");

  waitForClipboard(getExpectedClipboardText(HUD.outputNode.selectedItem),
    clipboard_setup, clipboard_copy_done, clipboard_copy_done);
}

function getExpectedClipboardText(aItem) {
  return "[" + ConsoleUtils.timestampString(aItem.timestamp) + "] " +
         aItem.clipboardText;
}
