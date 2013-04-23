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

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad(){
    browser.removeEventListener("load", onLoad, false);

    Services.prefs.setIntPref("devtools.hud.loglimit.cssparser", LOG_LIMIT);

    registerCleanupFunction(function() {
      Services.prefs.clearUserPref("devtools.hud.loglimit.cssparser");
    });

    openConsole(null, testCSSPruning);
  }, true);
}

function populateConsoleRepeats(aHudRef) {
  for (let i = 0; i < 5; i++) {
    let node = aHudRef.ui.createMessageNode(CATEGORY_CSS, SEVERITY_WARNING,
                                            "css log x");
    aHudRef.ui.outputMessage(CATEGORY_CSS, node);
  }
}

function populateConsole(aHudRef) {
  for (let i = 0; i < LOG_LIMIT + 5; i++) {
    let node = aHudRef.ui.createMessageNode(CATEGORY_CSS, SEVERITY_WARNING,
                                            "css log " + i);
    aHudRef.ui.outputMessage(CATEGORY_CSS, node);
  }
}

function testCSSPruning(hudRef) {
  populateConsoleRepeats(hudRef);

  let waitForNoRepeatedNodes = {
    name:  "number of nodes is LOG_LIMIT",
    validatorFn: function()
    {
      return countMessageNodes() == LOG_LIMIT;
    },
    successFn: function()
    {
      is(Object.keys(hudRef.ui._repeatNodes).length, LOG_LIMIT,
         "repeated nodes pruned from repeatNodes");

      let msg = hudRef.outputNode.querySelector(".webconsole-msg-cssparser " +
                                                ".webconsole-msg-repeat");
      is(msg.getAttribute("value"), 1,
         "repeated nodes pruned from repeatNodes (confirmed)");

      finishTest();
    },
    failureFn: finishTest,
  };

  waitForSuccess({
    name: "repeated nodes in cssNodes",
    validatorFn: function()
    {
      let msg = hudRef.outputNode.querySelector(".webconsole-msg-cssparser " +
                                                ".webconsole-msg-repeat");
      if (msg) {
        console.debug(msg, msg.getAttribute("value"));
      }
      return msg && msg.getAttribute("value") == 5;
    },
    successFn: function()
    {
      populateConsole(hudRef);
      waitForSuccess(waitForNoRepeatedNodes);
    },
    failureFn: finishTest,
  });
}

function countMessageNodes() {
  let outputNode = HUDService.getHudByWindow(content).outputNode;
  return outputNode.querySelectorAll(".hud-msg-node").length;
}
