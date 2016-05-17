/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that if a link without an onclick callback is clicked the link is
// opened in a new tab and no exception occurs (bug 999236).

"use strict";

function test() {
  function* runner() {
    const TEST_EVAL_STRING = "document";
    const TEST_PAGE_URI = "http://example.com/browser/devtools/client/" +
                          "webconsole/test/test-console.html";
    const {tab} = yield loadTab(TEST_PAGE_URI);
    const hud = yield openConsole(tab);

    hud.jsterm.execute(TEST_EVAL_STRING);

    const EXPECTED_OUTPUT = new RegExp("HTMLDocument \.+");

    let messages = yield waitForMessages({
      webconsole: hud,
      messages: [{
        name: "JS eval output",
        text: EXPECTED_OUTPUT,
        category: CATEGORY_OUTPUT,
      }],
    });

    let messageNode = messages[0].matched.values().next().value;

    // The correct anchor is second in the message node; the first anchor has
    // class .cm-variable. Ignore the first one by not matching anchors that
    // have the class .cm-variable.
    let urlNode = messageNode.querySelector("a:not(.cm-variable)");

    let linkOpened = false;
    let oldOpenUILinkIn = window.openUILinkIn;
    window.openUILinkIn = function (aLink) {
      if (aLink == TEST_PAGE_URI) {
        linkOpened = true;
      }
    };

    EventUtils.synthesizeMouseAtCenter(urlNode, {}, hud.iframeWindow);

    ok(linkOpened, "Clicking the URL opens the desired page");
    window.openUILinkIn = oldOpenUILinkIn;
  }

  Task.spawn(runner).then(finishTest);
}
