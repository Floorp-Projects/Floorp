/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the flexbox highlighter is hidden when the highlighted flexbox container is
// removed from the page.

const TEST_URI = `
  <style type='text/css'>
    #flex {
      display: flex;
    }
  </style>
  <div id="flex"></div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view, testActor} = yield openRuleView();
  let {highlighters} = view;

  yield selectNode("#flex", inspector);
  let container = getRuleViewProperty(view, "#flex", "display").valueSpan;
  let flexboxToggle = container.querySelector(".ruleview-flex");

  info("Toggling ON the flexbox highlighter from the rule-view.");
  let onHighlighterShown = highlighters.once("flexbox-highlighter-shown");
  flexboxToggle.click();
  yield onHighlighterShown;
  ok(highlighters.flexboxHighlighterShown, "Flexbox highlighter is shown.");

  info("Remove the #flex container in the content page.");
  let onHighlighterHidden = highlighters.once("flexbox-highlighter-hidden");
  testActor.eval(`document.querySelector("#flex").remove();`);
  yield onHighlighterHidden;
  ok(!highlighters.flexboxHighlighterShown, "Flexbox highlighter is hidden.");
});
