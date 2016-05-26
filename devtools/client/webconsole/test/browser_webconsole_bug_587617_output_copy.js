/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* globals goUpdateCommand goDoCommand */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

var HUD, outputNode;

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();
  yield consoleOpened(hud);
  yield testContextMenuCopy();

  HUD = outputNode = null;
});

function consoleOpened(hud) {
  HUD = hud;

  let deferred = promise.defer();

  // See bugs 574036, 586386 and 587617.
  outputNode = HUD.outputNode;

  HUD.jsterm.clearOutput();

  let controller = top.document.commandDispatcher
                               .getControllerForCommand("cmd_copy");
  is(controller.isCommandEnabled("cmd_copy"), false, "cmd_copy is disabled");

  ContentTask.spawn(gBrowser.selectedBrowser, null,
    "() => content.console.log('Hello world! bug587617')");

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

    // Remove new lines and whitespace since getSelection() includes
    // a new line between message and line number, but the clipboard doesn't
    // @see bug 1119503
    let selection = (HUD.iframeWindow.getSelection() + "")
      .replace(/\r?\n|\r| /g, "");
    isnot(selection.indexOf("bug587617"), -1,
          "selection text includes 'bug587617'");

    waitForClipboard((str) => {
      // Strip out spaces for comparison ease
      return selection.trim() == str.trim().replace(/ /g, "");
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

  let contextMenuId = HUD.ui.outputWrapper.getAttribute("context");
  let contextMenu = HUD.ui.document.getElementById(contextMenuId);
  ok(contextMenu, "the output node has a context menu");

  let copyItem = contextMenu.querySelector("*[command='cmd_copy']");
  ok(copyItem, "the context menu on the output node has a \"Copy\" item");

  // Remove new lines and whitespace since getSelection() includes
  // a new line between message and line number, but the clipboard doesn't
  // @see bug 1119503
  let selection = (HUD.iframeWindow.getSelection() + "")
    .replace(/\r?\n|\r| /g, "");

  copyItem.doCommand();

  waitForClipboard((str) => {
    // Strip out spaces for comparison ease
    return selection.trim() == str.trim().replace(/ /g, "");
  }, () => {
    goDoCommand("cmd_copy");
  }, deferred.resolve, deferred.resolve);
  HUD = outputNode = null;

  return deferred.promise;
}
