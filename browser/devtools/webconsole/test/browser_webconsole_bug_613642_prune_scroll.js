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
  let oldPref = Services.prefs.getIntPref("devtools.hud.loglimit.console");

  Services.prefs.setIntPref("devtools.hud.loglimit.console", 140);
  let scrollBoxElement = outputNode.scrollBoxObject.element;
  let boxObject = outputNode.scrollBoxObject;

  for (let i = 0; i < 150; i++) {
    hud.console.log("test message " + i);
  }

  let oldScrollTop = scrollBoxElement.scrollTop;
  ok(oldScrollTop > 0, "scroll location is not at the top");

  let firstNode = outputNode.firstChild;
  ok(firstNode, "found the first message");

  let msgNode = outputNode.querySelectorAll("richlistitem")[80];
  ok(msgNode, "found the 80th message");

  // scroll to the middle message node
  boxObject.ensureElementIsVisible(msgNode);

  isnot(scrollBoxElement.scrollTop, oldScrollTop,
        "scroll location updated (scrolled to message)");

  oldScrollTop = scrollBoxElement.scrollTop;

  // add a message
  hud.console.log("hello world");

  // Scroll location needs to change, because one message is also removed, and
  // we need to scroll a bit towards the top, to keep the current view in sync.
  isnot(scrollBoxElement.scrollTop, oldScrollTop,
        "scroll location updated (added a message)");

  isnot(outputNode.firstChild, firstNode,
        "first message removed");

  Services.prefs.setIntPref("devtools.hud.loglimit.console", oldPref);
  finishTest();
}

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 613642: maintain scroll with pruning of old messages");
  browser.addEventListener("load", tabLoad, true);
}
