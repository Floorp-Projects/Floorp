/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/browser/test-console.html";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();
  testCompletion(hud);
});

function testCompletion(hud) {
  let jsterm = hud.jsterm;
  let input = jsterm.inputNode;

  jsterm.setInputValue("");
  EventUtils.synthesizeKey("VK_TAB", {});
  is(jsterm.completeNode.value, "<- no result", "<- no result - matched");
  is(input.value, "", "inputnode is empty - matched");
  is(input.getAttribute("focused"), "true", "input is still focused");

  // Any thing which is not in property autocompleter
  jsterm.setInputValue("window.Bug583816");
  EventUtils.synthesizeKey("VK_TAB", {});
  is(jsterm.completeNode.value, "                <- no result",
     "completenode content - matched");
  is(input.value, "window.Bug583816", "inputnode content - matched");
  is(input.getAttribute("focused"), "true", "input is still focused");
}
