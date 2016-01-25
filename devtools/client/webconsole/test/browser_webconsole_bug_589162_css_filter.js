/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<div style='font-size:3em;" +
                 "foobarCssParser:baz'>test CSS parser filter</div>";

/**
 * Unit test for bug 589162:
 * CSS filtering on the console does not work
 */
function test() {
  Task.spawn(runner).then(finishTest);

  function* runner() {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);

    // CSS warnings are disabled by default.
    hud.setFilterState("cssparser", true);
    hud.jsterm.clearOutput();

    content.location.reload();

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
  }
}
