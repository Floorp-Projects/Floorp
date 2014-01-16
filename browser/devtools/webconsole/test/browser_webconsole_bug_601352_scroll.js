/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

function consoleOpened(HUD) {
  HUD.jsterm.clearOutput();

  let longMessage = "";
  for (let i = 0; i < 50; i++) {
    longMessage += "LongNonwrappingMessage";
  }

  for (let i = 0; i < 50; i++) {
    content.console.log("test message " + i);
  }

  content.console.log(longMessage);

  for (let i = 0; i < 50; i++) {
    content.console.log("test message " + i);
  }

  HUD.jsterm.execute("1+1", performTest);

  function performTest(node) {
    let scrollNode = HUD.outputNode.parentNode;
    let rectNode = node.getBoundingClientRect();
    let rectOutput = scrollNode.getBoundingClientRect();

    isnot(scrollNode.scrollTop, 0, "scroll location is not at the top");

    // Visible scroll viewport.
    let height = scrollNode.scrollHeight - scrollNode.scrollTop;

    // Top position of the last message node, relative to the outputNode.
    let top = rectNode.top + scrollNode.scrollTop;
    let bottom = top + node.clientHeight;
    info("output height " + height + " node top " + top + " node bottom " + bottom + " node height " + node.clientHeight);

    ok(top >= 0 && bottom <= height, "last message is visible");

    finishTest();
  };
}

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 601352");
  browser.addEventListener("load", function tabLoad(aEvent) {
    browser.removeEventListener(aEvent.type, tabLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

