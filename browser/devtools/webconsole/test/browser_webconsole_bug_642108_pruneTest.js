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

  waitForMessages({
    webconsole: hudRef,
    messages: [{
      text: "css log x",
      category: CATEGORY_CSS,
      severity: SEVERITY_WARNING,
      repeats: 5,
    }],
  }).then(() => {
    populateConsole(hudRef);
    waitForMessages({
      webconsole: hudRef,
      messages: [{
        text: "css log 0",
        category: CATEGORY_CSS,
        severity: SEVERITY_WARNING,
      },
      {
        text: "css log 24", // LOG_LIMIT + 5
        category: CATEGORY_CSS,
        severity: SEVERITY_WARNING,
      }],
    }).then(([result]) => {
      is(countMessageNodes(), LOG_LIMIT, "number of messages");

      is(Object.keys(hudRef.ui._repeatNodes).length, LOG_LIMIT,
         "repeated nodes pruned from repeatNodes");

      let msg = [...result.matched][0];
      let repeats = msg.querySelector(".repeats");
      is(repeats.getAttribute("value"), 1,
         "repeated nodes pruned from repeatNodes (confirmed)");

      finishTest();
    });
  });
}

function countMessageNodes() {
  let outputNode = HUDService.getHudByWindow(content).outputNode;
  return outputNode.querySelectorAll(".message").length;
}
