/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view content is correct

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8,browser_ruleview_content.js");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Creating the test document");
  let style = "" +
    "#testid {" +
    "  background-color: blue;" +
    "}" +
    ".testclass, .unmatched {" +
    "  background-color: green;" +
    "}";
  let styleNode = addStyle(content.document, style);
  content.document.body.innerHTML = "<div id='testid' class='testclass'>Styled Node</div>" +
                                    "<div id='testid2'>Styled Node</div>";

  yield testContentAfterNodeSelection(inspector, view);
});

function* testContentAfterNodeSelection(inspector, ruleView) {
  yield selectNode("#testid", inspector);
  is(ruleView.element.querySelectorAll("#noResults").length, 0,
    "After a highlight, no longer has a no-results element.");

  yield clearCurrentNodeSelection(inspector)
  is(ruleView.element.querySelectorAll("#noResults").length, 1,
    "After highlighting null, has a no-results element again.");

  yield selectNode("#testid", inspector);
  let classEditor = getRuleViewRuleEditor(ruleView, 2);
  is(classEditor.selectorText.querySelector(".ruleview-selector-matched").textContent,
    ".testclass", ".textclass should be matched.");
  is(classEditor.selectorText.querySelector(".ruleview-selector-unmatched").textContent,
    ".unmatched", ".unmatched should not be matched.");
}
