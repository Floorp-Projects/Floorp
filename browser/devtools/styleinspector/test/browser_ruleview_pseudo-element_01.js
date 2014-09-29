/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that pseudoelements are displayed correctly in the rule view

const TEST_URI = TEST_URL_ROOT + "doc_pseudoelement.html";

let test = asyncTest(function*() {
  yield addTab(TEST_URI);
  let {toolbox, inspector, view} = yield openRuleView();

  yield testTopLeft(inspector, view);
  yield testTopRight(inspector, view);
  yield testBottomRight(inspector, view);
  yield testBottomLeft(inspector, view);
  yield testParagraph(inspector, view);
  yield testBody(inspector, view);
});

function* testTopLeft(inspector, view) {
  let selector = "#topleft";
  let {
    rules,
    element,
    elementStyle
  } = yield assertPseudoElementRulesNumbers(selector, inspector, view, {
    elementRulesNb: 4,
    firstLineRulesNb: 2,
    firstLetterRulesNb: 1,
    selectionRulesNb: 0
  });

  let gutters = assertGutters(view);

  // Make sure that clicking on the twisty hides pseudo elements
  let expander = gutters[0].querySelector(".ruleview-expander");
  ok (view.element.firstChild.classList.contains("show-expandable-container"), "Pseudo Elements are expanded");
  expander.click();
  ok (!view.element.firstChild.classList.contains("show-expandable-container"), "Pseudo Elements are collapsed by twisty");
  expander.click();
  ok (view.element.firstChild.classList.contains("show-expandable-container"), "Pseudo Elements are expanded again");

  // Make sure that dblclicking on the header container also toggles the pseudo elements
  EventUtils.synthesizeMouseAtCenter(gutters[0], {clickCount: 2}, inspector.sidebar.getWindowForTab("ruleview"));
  ok (!view.element.firstChild.classList.contains("show-expandable-container"), "Pseudo Elements are collapsed by dblclicking");

  let defaultView = element.ownerDocument.defaultView;

  let elementRule = rules.elementRules[0];
  let elementRuleView = getRuleViewRuleEditor(view, 3);

  let elementFirstLineRule = rules.firstLineRules[0];
  let elementFirstLineRuleView = [].filter.call(view.element.children[1].children, (e) => {
    return e._ruleEditor && e._ruleEditor.rule === elementFirstLineRule;
  })[0]._ruleEditor;

  is
  (
    convertTextPropsToString(elementFirstLineRule.textProps),
    "color: orange",
    "TopLeft firstLine properties are correct"
  );

  let firstProp = elementFirstLineRuleView.addProperty("background-color", "rgb(0, 255, 0)", "");
  let secondProp = elementFirstLineRuleView.addProperty("font-style", "italic", "");

  is (firstProp, elementFirstLineRule.textProps[elementFirstLineRule.textProps.length - 2],
      "First added property is on back of array");
  is (secondProp, elementFirstLineRule.textProps[elementFirstLineRule.textProps.length - 1],
      "Second added property is on back of array");

  yield elementFirstLineRule._applyingModifications;

  is((yield getComputedStyleProperty(selector, ":first-line", "background-color")),
    "rgb(0, 255, 0)", "Added property should have been used.");
  is((yield getComputedStyleProperty(selector, ":first-line", "font-style")),
    "italic", "Added property should have been used.");
  is((yield getComputedStyleProperty(selector, null, "text-decoration")),
    "none", "Added property should not apply to element");

  firstProp.setEnabled(false);
  yield elementFirstLineRule._applyingModifications;

  is((yield getComputedStyleProperty(selector, ":first-line", "background-color")),
    "rgb(255, 0, 0)", "Disabled property should now have been used.");
  is((yield getComputedStyleProperty(selector, null, "background-color")),
    "rgb(221, 221, 221)", "Added property should not apply to element");

  firstProp.setEnabled(true);
  yield elementFirstLineRule._applyingModifications;

  is((yield getComputedStyleProperty(selector, ":first-line", "background-color")),
    "rgb(0, 255, 0)", "Added property should have been used.");
  is((yield getComputedStyleProperty(selector, null, "text-decoration")),
    "none", "Added property should not apply to element");

  firstProp = elementRuleView.addProperty("background-color", "rgb(0, 0, 255)", "");
  yield elementRule._applyingModifications;

  is((yield getComputedStyleProperty(selector, null, "background-color")),
    "rgb(0, 0, 255)", "Added property should have been used.");
  is((yield getComputedStyleProperty(selector, ":first-line", "background-color")),
    "rgb(0, 255, 0)", "Added prop does not apply to pseudo");
}

function* testTopRight(inspector, view) {
  let {
    rules,
    element,
    elementStyle
  } = yield assertPseudoElementRulesNumbers("#topright", inspector, view, {
    elementRulesNb: 4,
    firstLineRulesNb: 1,
    firstLetterRulesNb: 1,
    selectionRulesNb: 0
  });

  let gutters = assertGutters(view);

  let expander = gutters[0].querySelector(".ruleview-expander");
  ok (!view.element.firstChild.classList.contains("show-expandable-container"), "Pseudo Elements remain collapsed after switching element");
  expander.scrollIntoView();
  expander.click();
  ok (view.element.firstChild.classList.contains("show-expandable-container"), "Pseudo Elements are shown again after clicking twisty");
}

function* testBottomRight(inspector, view) {
  yield assertPseudoElementRulesNumbers("#bottomright", inspector, view, {
    elementRulesNb: 4,
    firstLineRulesNb: 1,
    firstLetterRulesNb: 1,
    selectionRulesNb: 0
  });
}

function* testBottomLeft(inspector, view) {
  yield assertPseudoElementRulesNumbers("#bottomleft", inspector, view, {
    elementRulesNb: 4,
    firstLineRulesNb: 1,
    firstLetterRulesNb: 1,
    selectionRulesNb: 0
  });
}

function* testParagraph(inspector, view) {
  let {
    rules,
    element,
    elementStyle
  } = yield assertPseudoElementRulesNumbers("#bottomleft p", inspector, view, {
    elementRulesNb: 3,
    firstLineRulesNb: 1,
    firstLetterRulesNb: 1,
    selectionRulesNb: 1
  });

  let gutters = assertGutters(view);

  let elementFirstLineRule = rules.firstLineRules[0];
  let elementFirstLineRuleView = [].filter.call(view.element.children[1].children, (e) => {
    return e._ruleEditor && e._ruleEditor.rule === elementFirstLineRule;
  })[0]._ruleEditor;

  is
  (
    convertTextPropsToString(elementFirstLineRule.textProps),
    "background: none repeat scroll 0% 0% blue",
    "Paragraph first-line properties are correct"
  );

  let elementFirstLetterRule = rules.firstLetterRules[0];
  let elementFirstLetterRuleView = [].filter.call(view.element.children[1].children, (e) => {
    return e._ruleEditor && e._ruleEditor.rule === elementFirstLetterRule;
  })[0]._ruleEditor;

  is
  (
    convertTextPropsToString(elementFirstLetterRule.textProps),
    "color: red; font-size: 130%",
    "Paragraph first-letter properties are correct"
  );

  let elementSelectionRule = rules.selectionRules[0];
  let elementSelectionRuleView = [].filter.call(view.element.children[1].children, (e) => {
    return e._ruleEditor && e._ruleEditor.rule === elementSelectionRule;
  })[0]._ruleEditor;

  is
  (
    convertTextPropsToString(elementSelectionRule.textProps),
    "color: white; background: none repeat scroll 0% 0% black",
    "Paragraph first-letter properties are correct"
  );
}

function* testBody(inspector, view) {
  let {element, elementStyle} = yield testNode("body", inspector, view);

  let gutters = view.element.querySelectorAll(".theme-gutter");
  is (gutters.length, 0, "There are no gutter headings");
}

function convertTextPropsToString(textProps) {
  return textProps.map(t => t.name + ": " + t.value).join("; ");
}

function* testNode(selector, inspector, view) {
  let element = getNode(selector);
  yield selectNode(selector, inspector);
  let elementStyle = view._elementStyle;
  return {element: element, elementStyle: elementStyle};
}

function* assertPseudoElementRulesNumbers(selector, inspector, view, ruleNbs) {
  let {element, elementStyle} = yield testNode(selector, inspector, view);

  let rules = {
    elementRules: elementStyle.rules.filter(rule => !rule.pseudoElement),
    firstLineRules: elementStyle.rules.filter(rule => rule.pseudoElement === ":first-line"),
    firstLetterRules: elementStyle.rules.filter(rule => rule.pseudoElement === ":first-letter"),
    selectionRules: elementStyle.rules.filter(rule => rule.pseudoElement === ":-moz-selection")
  };

  is(rules.elementRules.length, ruleNbs.elementRulesNb, selector +
    " has the correct number of non pseudo element rules");
  is(rules.firstLineRules.length, ruleNbs.firstLineRulesNb, selector +
    " has the correct number of :first-line rules");
  is(rules.firstLetterRules.length, ruleNbs.firstLetterRulesNb, selector +
    " has the correct number of :first-letter rules");
  is(rules.selectionRules.length, ruleNbs.selectionRulesNb, selector +
    " has the correct number of :selection rules");

  return {rules: rules, element: element, elementStyle: elementStyle};
}

function assertGutters(view) {
  let gutters = view.element.querySelectorAll(".theme-gutter");
  is (gutters.length, 3, "There are 3 gutter headings");
  is (gutters[0].textContent, "Pseudo-elements", "Gutter heading is correct");
  is (gutters[1].textContent, "This Element", "Gutter heading is correct");
  is (gutters[2].textContent, "Inherited from body", "Gutter heading is correct");

  return gutters;
}