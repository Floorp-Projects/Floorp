/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that the Web Console limits the number of lines displayed according to
// the user's preferences.

const TEST_URI = "data:text/html;charset=utf-8,<p>test for bug 642108.";
const LOG_LIMIT = 20;
const CATEGORY_CSS = 1;
const SEVERITY_WARNING = 1;

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testCSSPruning, false);
}

function populateConsoleRepeats(aHudRef) {
  let hud = aHudRef.HUDBox;

  for (let i = 0; i < 5; i++) {
    let node = ConsoleUtils.createMessageNode(hud.ownerDocument,
                                              CATEGORY_CSS,
                                              SEVERITY_WARNING,
                                              "css log x",
                                              aHudRef.hudId);
    ConsoleUtils.outputMessageNode(node, aHudRef.hudId);
  }
}


function populateConsole(aHudRef) {
  let hud = aHudRef.HUDBox;

  for (let i = 0; i < LOG_LIMIT + 5; i++) {
    let node = ConsoleUtils.createMessageNode(hud.ownerDocument,
                                              CATEGORY_CSS,
                                              SEVERITY_WARNING,
                                              "css log " + i,
                                              aHudRef.hudId);
    ConsoleUtils.outputMessageNode(node, aHudRef.hudId);
  }
}

function testCSSPruning() {
  let prefBranch = Services.prefs.getBranch("devtools.hud.loglimit.");
  prefBranch.setIntPref("cssparser", LOG_LIMIT);

  browser.removeEventListener("DOMContentLoaded",testCSSPruning, false);

  openConsole();
  let hudRef = HUDService.getHudByWindow(content);

  populateConsoleRepeats(hudRef);
  ok(hudRef.cssNodes["css log x"], "repeated nodes in cssNodes");

  populateConsole(hudRef);

  is(countMessageNodes(), LOG_LIMIT, "number of nodes is LOG_LIMIT");
  ok(!hudRef.cssNodes["css log x"], "repeated nodes pruned from cssNodes");

  prefBranch.clearUserPref("loglimit");
  prefBranch = null;

  finishTest();
}

function countMessageNodes() {
  let outputNode = HUDService.getHudByWindow(content).outputNode;
  return outputNode.querySelectorAll(".hud-msg-node").length;
}
