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
  let {
    rules,
    element,
    elementStyle
  } = yield assertPseudoElementRulesNumbers("#topleft", inspector, view, {
    elementRulesNb: 4,
    afterRulesNb: 1,
    beforeRulesNb: 2,
    firstLineRulesNb: 0,
    firstLetterRulesNb: 0,
    selectionRulesNb: 0
  });

  let gutters = assertGutters(view);

  // Make sure that clicking on the twisty hides pseudo elements
  let expander = gutters[0].querySelector(".ruleview-expander");
  ok (view.element.classList.contains("show-pseudo-elements"), "Pseudo Elements are expanded");
  expander.click();
  ok (!view.element.classList.contains("show-pseudo-elements"), "Pseudo Elements are collapsed by twisty");
  expander.click();
  ok (view.element.classList.contains("show-pseudo-elements"), "Pseudo Elements are expanded again");

  // Make sure that dblclicking on the header container also toggles the pseudo elements
  EventUtils.synthesizeMouseAtCenter(gutters[0], {clickCount: 2}, inspector.sidebar.getWindowForTab("ruleview"));
  ok (!view.element.classList.contains("show-pseudo-elements"), "Pseudo Elements are collapsed by dblclicking");

  let defaultView = element.ownerDocument.defaultView;
  let elementRule = rules.elementRules[0];
  let elementRuleView = [].filter.call(view.element.children, e => {
    return e._ruleEditor && e._ruleEditor.rule === elementRule;
  })[0]._ruleEditor;

  let elementAfterRule = rules.afterRules[0];
  let elementAfterRuleView = [].filter.call(view.element.children, (e) => {
    return e._ruleEditor && e._ruleEditor.rule === elementAfterRule;
  })[0]._ruleEditor;

  is
  (
    convertTextPropsToString(elementAfterRule.textProps),
    "background: none repeat scroll 0% 0% red; content: \" \"; position: absolute; " +
    "border-radius: 50%; height: 32px; width: 32px; top: 50%; left: 50%; margin-top: -16px; margin-left: -16px",
    "TopLeft after properties are correct"
  );

  let elementBeforeRule = rules.beforeRules[0];
  let elementBeforeRuleView = [].filter.call(view.element.children, (e) => {
    return e._ruleEditor && e._ruleEditor.rule === elementBeforeRule;
  })[0]._ruleEditor;

  is
  (
    convertTextPropsToString(elementBeforeRule.textProps),
    "top: 0px; left: 0px",
    "TopLeft before properties are correct"
  );

  let firstProp = elementAfterRuleView.addProperty("background-color", "rgb(0, 255, 0)", "");
  let secondProp = elementAfterRuleView.addProperty("padding", "100px", "");

  is (firstProp, elementAfterRule.textProps[elementAfterRule.textProps.length - 2],
      "First added property is on back of array");
  is (secondProp, elementAfterRule.textProps[elementAfterRule.textProps.length - 1],
      "Second added property is on back of array");

  yield elementAfterRule._applyingModifications;

  is(defaultView.getComputedStyle(element, ":after").getPropertyValue("background-color"),
    "rgb(0, 255, 0)", "Added property should have been used.");
  is(defaultView.getComputedStyle(element, ":after").getPropertyValue("padding-top"),
    "100px", "Added property should have been used.");
  is(defaultView.getComputedStyle(element).getPropertyValue("padding-top"),
    "32px", "Added property should not apply to element");

  secondProp.setEnabled(false);
  yield elementAfterRule._applyingModifications;

  is(defaultView.getComputedStyle(element, ":after").getPropertyValue("padding-top"), "0px",
    "Disabled property should have been used.");
  is(defaultView.getComputedStyle(element).getPropertyValue("padding-top"), "32px",
    "Added property should not apply to element");

  secondProp.setEnabled(true);
  yield elementAfterRule._applyingModifications;

  is(defaultView.getComputedStyle(element, ":after").getPropertyValue("padding-top"), "100px",
    "Enabled property should have been used.");
  is(defaultView.getComputedStyle(element).getPropertyValue("padding-top"), "32px",
    "Added property should not apply to element");

  let firstProp = elementRuleView.addProperty("background-color", "rgb(0, 0, 255)", "");
  yield elementRule._applyingModifications;

  is(defaultView.getComputedStyle(element).getPropertyValue("background-color"), "rgb(0, 0, 255)",
    "Added property should have been used.");
  is(defaultView.getComputedStyle(element, ":after").getPropertyValue("background-color"), "rgb(0, 255, 0)",
    "Added prop does not apply to pseudo");
}

function* testTopRight(inspector, view) {
  let {
    rules,
    element,
    elementStyle
  } = yield assertPseudoElementRulesNumbers("#topright", inspector, view, {
    elementRulesNb: 4,
    afterRulesNb: 1,
    beforeRulesNb: 2,
    firstLineRulesNb: 0,
    firstLetterRulesNb: 0,
    selectionRulesNb: 0
  });

  let gutters = assertGutters(view);

  let expander = gutters[0].querySelector(".ruleview-expander");
  ok (!view.element.classList.contains("show-pseudo-elements"), "Pseudo Elements remain collapsed after switching element");
  expander.scrollIntoView();
  expander.click();
  ok (view.element.classList.contains("show-pseudo-elements"), "Pseudo Elements are shown again after clicking twisty");
}

function* testBottomRight(inspector, view) {
  yield assertPseudoElementRulesNumbers("#bottomright", inspector, view, {
    elementRulesNb: 4,
    afterRulesNb: 1,
    beforeRulesNb: 3,
    firstLineRulesNb: 0,
    firstLetterRulesNb: 0,
    selectionRulesNb: 0
  });
}

function* testBottomLeft(inspector, view) {
  yield assertPseudoElementRulesNumbers("#bottomleft", inspector, view, {
    elementRulesNb: 4,
    afterRulesNb: 1,
    beforeRulesNb: 2,
    firstLineRulesNb: 0,
    firstLetterRulesNb: 0,
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
    afterRulesNb: 0,
    beforeRulesNb: 0,
    firstLineRulesNb: 1,
    firstLetterRulesNb: 1,
    selectionRulesNb: 1
  });

  let gutters = assertGutters(view);

  let elementFirstLineRule = rules.firstLineRules[0];
  let elementFirstLineRuleView = [].filter.call(view.element.children, (e) => {
    return e._ruleEditor && e._ruleEditor.rule === elementFirstLineRule;
  })[0]._ruleEditor;

  is
  (
    convertTextPropsToString(elementFirstLineRule.textProps),
    "background: none repeat scroll 0% 0% blue",
    "Paragraph first-line properties are correct"
  );

  let elementFirstLetterRule = rules.firstLetterRules[0];
  let elementFirstLetterRuleView = [].filter.call(view.element.children, (e) => {
    return e._ruleEditor && e._ruleEditor.rule === elementFirstLetterRule;
  })[0]._ruleEditor;

  is
  (
    convertTextPropsToString(elementFirstLetterRule.textProps),
    "color: red; font-size: 130%",
    "Paragraph first-letter properties are correct"
  );

  let elementSelectionRule = rules.selectionRules[0];
  let elementSelectionRuleView = [].filter.call(view.element.children, (e) => {
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
  yield selectNode(element, inspector);
  let elementStyle = view._elementStyle;
  return {element: element, elementStyle: elementStyle};
}

function* assertPseudoElementRulesNumbers(selector, inspector, view, ruleNbs) {
  let {element, elementStyle} = yield testNode(selector, inspector, view);

  let rules = {
    elementRules: elementStyle.rules.filter(rule => !rule.pseudoElement),
    afterRules: elementStyle.rules.filter(rule => rule.pseudoElement === ":after"),
    beforeRules: elementStyle.rules.filter(rule => rule.pseudoElement === ":before"),
    firstLineRules: elementStyle.rules.filter(rule => rule.pseudoElement === ":first-line"),
    firstLetterRules: elementStyle.rules.filter(rule => rule.pseudoElement === ":first-letter"),
    selectionRules: elementStyle.rules.filter(rule => rule.pseudoElement === ":-moz-selection")
  };

  is(rules.elementRules.length, ruleNbs.elementRulesNb, selector +
    " has the correct number of non pseudo element rules");
  is(rules.afterRules.length, ruleNbs.afterRulesNb, selector +
    " has the correct number of :after rules");
  is(rules.beforeRules.length, ruleNbs.beforeRulesNb, selector +
    " has the correct number of :before rules");
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
