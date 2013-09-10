/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Șucan <mihai.sucan@gmail.com>
 */

let hud, testDriver;

function testNext() {
  testDriver.next();
}

function testGen() {
  hud.jsterm.clearOutput();

  let outputNode = hud.outputNode;

  Services.prefs.setIntPref("devtools.hud.loglimit.console", 140);
  let scrollBoxElement = outputNode.parentNode;

  for (let i = 0; i < 150; i++) {
    content.console.log("test message " + i);
  }

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test message 149",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(testNext);

  yield undefined;

  let oldScrollTop = scrollBoxElement.scrollTop;
  isnot(oldScrollTop, 0, "scroll location is not at the top");

  let firstNode = outputNode.firstChild;
  ok(firstNode, "found the first message");

  let msgNode = outputNode.children[80];
  ok(msgNode, "found the 80th message");

  // scroll to the middle message node
  msgNode.scrollIntoView(false);

  isnot(scrollBoxElement.scrollTop, oldScrollTop,
        "scroll location updated (scrolled to message)");

  oldScrollTop = scrollBoxElement.scrollTop;

  // add a message
  content.console.log("hello world");

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "hello world",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(testNext);

  yield undefined;

  // Scroll location needs to change, because one message is also removed, and
  // we need to scroll a bit towards the top, to keep the current view in sync.
  isnot(scrollBoxElement.scrollTop, oldScrollTop,
        "scroll location updated (added a message)");

  isnot(outputNode.firstChild, firstNode,
        "first message removed");

  Services.prefs.clearUserPref("devtools.hud.loglimit.console");

  hud = testDriver = null;
  finishTest();

  yield undefined;
}

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 613642: maintain scroll with pruning of old messages");
  browser.addEventListener("load", function tabLoad(aEvent) {
    browser.removeEventListener(aEvent.type, tabLoad, true);

    openConsole(null, function(aHud) {
      hud = aHud;
      testDriver = testGen();
      testDriver.next();
    });
  }, true);
}
