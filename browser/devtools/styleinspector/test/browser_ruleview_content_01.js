/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view content is correct

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,browser_ruleview_content.js");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Creating the test document");
  let style = "" +
    "@media screen and (min-width: 10px) {" +
    "  #testid {" +
    "    background-color: blue;" +
    "  }" +
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

  let linkText = getRuleViewLinkTextByIndex(ruleView, 1);
  is(linkText, "inline:1 @screen and (min-width: 10px)",
    "link text at index 1 contains media query text.");

  linkText = getRuleViewLinkTextByIndex(ruleView, 2);
  is(linkText, "inline:1",
    "link text at index 2 contains no media query text.");

  let classEditor = getRuleViewRuleEditor(ruleView, 2);
  is(classEditor.selectorText.querySelector(".ruleview-selector-matched").textContent,
    ".testclass", ".textclass should be matched.");
  is(classEditor.selectorText.querySelector(".ruleview-selector-unmatched").textContent,
    ".unmatched", ".unmatched should not be matched.");
}
