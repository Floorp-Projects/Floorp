/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the Web Console limits the number of lines displayed according to
// the user's preferences.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>test for bug 642108.";
const LOG_LIMIT = 20;

function test() {
  let hud;

  Task.spawn(runner).then(finishTest);

  function* runner() {
    let {tab} = yield loadTab(TEST_URI);

    Services.prefs.setIntPref("devtools.hud.loglimit.cssparser", LOG_LIMIT);
    Services.prefs.setBoolPref("devtools.webconsole.filter.cssparser", true);

    registerCleanupFunction(function () {
      Services.prefs.clearUserPref("devtools.hud.loglimit.cssparser");
      Services.prefs.clearUserPref("devtools.webconsole.filter.cssparser");
    });

    hud = yield openConsole(tab);

    for (let i = 0; i < 5; i++) {
      logCSSMessage("css log x");
    }

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "css log x",
        category: CATEGORY_CSS,
        severity: SEVERITY_WARNING,
        repeats: 5,
      }],
    });

    for (let i = 0; i < LOG_LIMIT + 5; i++) {
      logCSSMessage("css log " + i);
    }

    let [result] = yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "css log 5",
        category: CATEGORY_CSS,
        severity: SEVERITY_WARNING,
      },
        {
        // LOG_LIMIT + 5
          text: "css log 24",
          category: CATEGORY_CSS,
          severity: SEVERITY_WARNING,
        }],
    });

    is(hud.ui.outputNode.querySelectorAll(".message").length, LOG_LIMIT,
       "number of messages");

    is(Object.keys(hud.ui._repeatNodes).length, LOG_LIMIT,
       "repeated nodes pruned from repeatNodes");

    let msg = [...result.matched][0];
    let repeats = msg.querySelector(".message-repeats");
    is(repeats.getAttribute("value"), 1,
       "repeated nodes pruned from repeatNodes (confirmed)");
  }

  function logCSSMessage(msg) {
    let node = hud.ui.createMessageNode(CATEGORY_CSS, SEVERITY_WARNING, msg);
    hud.ui.outputMessage(CATEGORY_CSS, node);
  }
}
