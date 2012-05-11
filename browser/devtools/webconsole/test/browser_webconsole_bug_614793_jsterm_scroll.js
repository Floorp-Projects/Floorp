/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Șucan <mihai.sucan@gmail.com>
 */

function tabLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  let hud = HUDService.hudReferences[hudId];
  let outputNode = hud.outputNode;
  let boxObject = outputNode.scrollBoxObject.element;

  for (let i = 0; i < 150; i++) {
    hud.console.log("test message " + i);
  }

  let oldScrollTop = boxObject.scrollTop;
  ok(oldScrollTop > 0, "scroll location is not at the top");

  hud.jsterm.execute("'hello world'");

  isnot(boxObject.scrollTop, oldScrollTop, "scroll location updated");

  oldScrollTop = boxObject.scrollTop;
  outputNode.scrollBoxObject.ensureElementIsVisible(outputNode.lastChild);

  is(boxObject.scrollTop, oldScrollTop, "scroll location is the same");

  finishTest();
}

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 614793: jsterm result scroll");
  browser.addEventListener("load", tabLoad, true);
}

