/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

var HUD, outputNode;

var test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();
  yield consoleOpened(hud);
  yield testContextMenuCopy();

  HUD = outputNode = null;
});

function consoleOpened(aHud) {
  HUD = aHud;

  let deferred = promise.defer();

  // See bugs 574036, 586386 and 587617.
  outputNode = HUD.outputNode;

  HUD.jsterm.clearOutput();

  let controller = top.document.commandDispatcher
                               .getControllerForCommand("cmd_copy");
  is(controller.isCommandEnabled("cmd_copy"), false, "cmd_copy is disabled");

  content.console.log("Hello world! bug587617");

  waitForMessages({
    webconsole: HUD,
    messages: [{
      text: "bug587617",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(([result]) => {
    let msg = [...result.matched][0];
    HUD.ui.output.selectMessage(msg);

    outputNode.focus();

    goUpdateCommand("cmd_copy");
    controller = top.document.commandDispatcher
                             .getControllerForCommand("cmd_copy");
    is(controller.isCommandEnabled("cmd_copy"), true, "cmd_copy is enabled");

    // Remove new lines since getSelection() includes one between message and
    // line number, but the clipboard doesn't (see bug 1119503)
    let selection = (HUD.iframeWindow.getSelection() + "")
      .replace(/\r?\n|\r/g, " ");
    isnot(selection.indexOf("bug587617"), -1,
          "selection text includes 'bug587617'");

    waitForClipboard((str) => {
      return selection.trim() == str.trim();
    }, () => {
      goDoCommand("cmd_copy");
    }, deferred.resolve, deferred.resolve);
  });
  return deferred.promise;
}

// Test that the context menu "Copy" (which has a different code path) works
// properly as well.
function testContextMenuCopy() {
  let deferred = promise.defer();

  let contextMenuId = outputNode.parentNode.getAttribute("context");
  let contextMenu = HUD.ui.document.getElementById(contextMenuId);
  ok(contextMenu, "the output node has a context menu");

  let copyItem = contextMenu.querySelector("*[command='cmd_copy']");
  ok(copyItem, "the context menu on the output node has a \"Copy\" item");

  // Remove new lines since getSelection() includes one between message and line
  // number, but the clipboard doesn't (see bug 1119503)
  let selection = (HUD.iframeWindow.getSelection() + "")
    .replace(/\r?\n|\r/g, " ");

  copyItem.doCommand();

  waitForClipboard((str) => {
    return selection.trim() == str.trim();
  }, () => {
    goDoCommand("cmd_copy");
  }, deferred.resolve, deferred.resolve);
  HUD = outputNode = null;

  return deferred.promise;
}
