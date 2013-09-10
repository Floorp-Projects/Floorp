/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Sucan <mihai.sucan@gmail.com>
 */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-621644-jsterm-dollar.html";

function test$(HUD) {
  HUD.jsterm.clearOutput();

  HUD.jsterm.execute("$(document.body)", (msg) => {
    ok(msg.textContent.indexOf("<p>") > -1,
       "jsterm output is correct for $()");

    test$$(HUD);
  });
}

function test$$(HUD) {
  HUD.jsterm.clearOutput();

  HUD.jsterm.setInputValue();
  HUD.jsterm.execute("$$(document)", (msg) => {
    ok(msg.textContent.indexOf("621644") > -1,
       "jsterm output is correct for $$()");
    finishTest();
  });
}

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, test$);
  }, true);
}
