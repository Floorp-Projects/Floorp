/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Șucan <mihai.sucan@gmail.com>
 */

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 613280";

function test() {
  loadTab(TEST_URI).then(() => {
    openConsole().then((HUD) => {
      content.console.log("foobarBazBug613280");
      waitForMessages({
        webconsole: HUD,
        messages: [{
          text: "foobarBazBug613280",
          category: CATEGORY_WEBDEV,
          severity: SEVERITY_LOG,
        }],
      }).then(performTest.bind(null, HUD));
    })
  });
}

function performTest(HUD, [result]) {
  let msg = [...result.matched][0];
  let input = HUD.jsterm.inputNode;
  let selection = getSelection();
  let contentSelection = content.getSelection();

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

  HUD.ui.output.selectMessage(msg);
  HUD.outputNode.focus();

  goUpdateCommand("cmd_copy");

  controller = top.document.commandDispatcher.
               getControllerForCommand("cmd_copy");
  is(controller.isCommandEnabled("cmd_copy"), true, "cmd_copy is enabled");

  // Remove new lines since getSelection() includes one between message and line
  // number, but the clipboard doesn't (see bug 1119503)
  let selectionText = (HUD.iframeWindow.getSelection() + "").replace(/\r?\n|\r/g, " ");
  isnot(selectionText.indexOf("foobarBazBug613280"), -1,
        "selection text includes 'foobarBazBug613280'");

  waitForClipboard((str) => { return str.trim() == selectionText.trim(); },
                   clipboard_setup, clipboard_copy_done, clipboard_copy_done);
}
