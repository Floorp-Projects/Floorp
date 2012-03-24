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
  let scrollBox = outputNode.scrollBoxObject.element;

  for (let i = 0; i < 150; i++) {
    hud.console.log("test message " + i);
  }

  let oldScrollTop = scrollBox.scrollTop;
  ok(oldScrollTop > 0, "scroll location is not at the top");

  // scroll to the first node
  outputNode.focus();

  EventUtils.synthesizeKey("VK_HOME", {});

  let topPosition = scrollBox.scrollTop;
  isnot(topPosition, oldScrollTop, "scroll location updated (moved to top)");

  executeSoon(function() {
    // add a message and make sure scroll doesn't change
    hud.console.log("test message 150");

    is(scrollBox.scrollTop, topPosition, "scroll location is still at the top");

    // scroll back to the bottom
    outputNode.lastChild.focus();
    EventUtils.synthesizeKey("VK_END", {});

    executeSoon(function() {
      oldScrollTop = outputNode.scrollTop;

      hud.console.log("test message 151");

      isnot(scrollBox.scrollTop, oldScrollTop,
            "scroll location updated (moved to bottom)");

      finishTest();
    });
  });
}

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 613642: remember scroll location");
  browser.addEventListener("load", tabLoad, true);
}
