/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Șucan <mihai.sucan@gmail.com>
 */

function consoleOpened(hud) {
  hud.jsterm.clearOutput();

  let scrollNode = hud.outputNode.parentNode;

  for (let i = 0; i < 150; i++) {
    content.console.log("test message " + i);
  }

  let oldScrollTop = -1;

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test message 149",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(() => {
    oldScrollTop = scrollNode.scrollTop;
    isnot(oldScrollTop, 0, "scroll location is not at the top");

    hud.jsterm.execute("'hello world'", onExecute);
  });

  function onExecute(msg)
  {
    isnot(scrollNode.scrollTop, oldScrollTop, "scroll location updated");

    oldScrollTop = scrollNode.scrollTop;

    msg.scrollIntoView(false);

    is(scrollNode.scrollTop, oldScrollTop, "scroll location is the same");

    finishTest();
  }
}

function test() {
  addTab("data:text/html;charset=utf-8,Web Console test for bug 614793: jsterm result scroll");
  browser.addEventListener("load", function onLoad(aEvent) {
    browser.removeEventListener(aEvent.type, onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

