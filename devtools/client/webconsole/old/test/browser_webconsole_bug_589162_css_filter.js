/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<div style='font-size:3em;" +
                 "foobarCssParser:baz'>test CSS parser filter</div>";

/**
 * Unit test for bug 589162:
 * CSS filtering on the console does not work
 */
add_task(function* () {
  let {tab} = yield loadTab(TEST_URI);
  let hud = yield openConsole(tab);

  // CSS warnings are disabled by default.
  hud.setFilterState("cssparser", true);
  hud.jsterm.clearOutput();

  BrowserReload();

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foobarCssParser",
      category: CATEGORY_CSS,
      severity: SEVERITY_WARNING,
    }],
  });

  hud.setFilterState("cssparser", false);

  let msg = "the unknown CSS property warning is not displayed, " +
            "after filtering";
  testLogEntry(hud.outputNode, "foobarCssParser", msg, true, true);
});
