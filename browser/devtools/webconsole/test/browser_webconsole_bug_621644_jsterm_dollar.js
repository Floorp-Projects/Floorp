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

  HUD.jsterm.setInputValue("$(document.body)");
  HUD.jsterm.execute();

  waitForSuccess({
    name: "jsterm output for $()",
    validatorFn: function()
    {
      return HUD.outputNode.querySelector(".webconsole-msg-output:last-child");
    },
    successFn: function()
    {
      let outputItem = HUD.outputNode.
                       querySelector(".webconsole-msg-output:last-child");
      ok(outputItem.textContent.indexOf("<p>") > -1,
         "jsterm output is correct for $()");

      test$$(HUD);
    },
    failureFn: test$$.bind(null, HUD),
  });
}

function test$$(HUD) {
  HUD.jsterm.clearOutput();

  HUD.jsterm.setInputValue("$$(document)");
  HUD.jsterm.execute();

  waitForSuccess({
    name: "jsterm output for $$()",
    validatorFn: function()
    {
      return HUD.outputNode.querySelector(".webconsole-msg-output:last-child");
    },
    successFn: function()
    {
      let outputItem = HUD.outputNode.
                       querySelector(".webconsole-msg-output:last-child");
      ok(outputItem.textContent.indexOf("621644") > -1,
         "jsterm output is correct for $$()");

      executeSoon(finishTest);
    },
    failureFn: finishTest,
  });
}

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, test$);
  }, true);
}
