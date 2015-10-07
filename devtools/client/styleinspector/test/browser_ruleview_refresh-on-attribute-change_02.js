/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changing the current element's style attribute refreshes the
// rule-view

const TEST_URI = `
  <div id="testid" class="testclass" style="margin-top: 1px; padding-top: 5px;">
    Styled Node
  </div>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  let testElement = getNode("#testid");
  yield selectNode("#testid", inspector);

  yield testPropertyChanges(inspector, view);
  yield testPropertyChange0(inspector, view, testElement);
  yield testPropertyChange1(inspector, view, testElement);
  yield testPropertyChange2(inspector, view, testElement);
  yield testPropertyChange3(inspector, view, testElement);
  yield testPropertyChange4(inspector, view, testElement);
  yield testPropertyChange5(inspector, view, testElement);
  yield testPropertyChange6(inspector, view, testElement);
});

function* testPropertyChanges(inspector, ruleView) {
  info("Adding a second margin-top value in the element selector");
  let ruleEditor = ruleView._elementStyle.rules[0].editor;
  let onRefreshed = inspector.once("rule-view-refreshed");
  ruleEditor.addProperty("margin-top", "5px", "");
  yield onRefreshed;

  let rule = ruleView._elementStyle.rules[0];
  validateTextProp(rule.textProps[0], false, "margin-top", "1px",
    "Original margin property active");
}

function* testPropertyChange0(inspector, ruleView, testElement) {
  yield changeElementStyle(testElement, "margin-top: 1px; padding-top: 5px",
    inspector);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3,
    "Correct number of properties");
  validateTextProp(rule.textProps[0], true, "margin-top", "1px",
    "First margin property re-enabled");
  validateTextProp(rule.textProps[2], false, "margin-top", "5px",
    "Second margin property disabled");
}

function* testPropertyChange1(inspector, ruleView, testElement) {
  info("Now set it back to 5px, the 5px value should be re-enabled.");
  yield changeElementStyle(testElement, "margin-top: 5px; padding-top: 5px;",
    inspector);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3,
    "Correct number of properties");
  validateTextProp(rule.textProps[0], false, "margin-top", "1px",
    "First margin property re-enabled");
  validateTextProp(rule.textProps[2], true, "margin-top", "5px",
    "Second margin property disabled");
}

function* testPropertyChange2(inspector, ruleView, testElement) {
  info("Set the margin property to a value that doesn't exist in the editor.");
  info("Should reuse the currently-enabled element (the second one.)");
  yield changeElementStyle(testElement, "margin-top: 15px; padding-top: 5px;",
    inspector);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3,
    "Correct number of properties");
  validateTextProp(rule.textProps[0], false, "margin-top", "1px",
    "First margin property re-enabled");
  validateTextProp(rule.textProps[2], true, "margin-top", "15px",
    "Second margin property disabled");
}

function* testPropertyChange3(inspector, ruleView, testElement) {
  info("Remove the padding-top attribute. Should disable the padding " +
    "property but not remove it.");
  yield changeElementStyle(testElement, "margin-top: 5px;", inspector);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3,
    "Correct number of properties");
  validateTextProp(rule.textProps[1], false, "padding-top", "5px",
    "Padding property disabled");
}

function* testPropertyChange4(inspector, ruleView, testElement) {
  info("Put the padding-top attribute back in, should re-enable the " +
    "padding property.");
  yield changeElementStyle(testElement, "margin-top: 5px; padding-top: 25px",
    inspector);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3,
    "Correct number of properties");
  validateTextProp(rule.textProps[1], true, "padding-top", "25px",
    "Padding property enabled");
}

function* testPropertyChange5(inspector, ruleView, testElement) {
  info("Add an entirely new property");
  yield changeElementStyle(testElement,
    "margin-top: 5px; padding-top: 25px; padding-left: 20px;", inspector);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 4,
    "Added a property");
  validateTextProp(rule.textProps[3], true, "padding-left", "20px",
    "Padding property enabled");
}

function* testPropertyChange6(inspector, ruleView, testElement) {
  info("Add an entirely new property again");
  yield changeElementStyle(testElement, "background: red " +
    "url(\"chrome://branding/content/about-logo.png\") repeat scroll 0% 0%",
    inspector);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 5,
    "Added a property");
  validateTextProp(rule.textProps[4], true, "background",
                   "red url(\"chrome://branding/content/about-logo.png\") repeat scroll 0% 0%",
                   "shortcut property correctly set");
}

function* changeElementStyle(testElement, style, inspector) {
  let onRefreshed = inspector.once("rule-view-refreshed");
  testElement.setAttribute("style", style);
  yield onRefreshed;
}

function validateTextProp(aProp, aEnabled, aName, aValue, aDesc) {
  is(aProp.enabled, aEnabled, aDesc + ": enabled.");
  is(aProp.name, aName, aDesc + ": name.");
  is(aProp.value, aValue, aDesc + ": value.");

  is(aProp.editor.enable.hasAttribute("checked"), aEnabled,
    aDesc + ": enabled checkbox.");
  is(aProp.editor.nameSpan.textContent, aName, aDesc + ": name span.");
  is(aProp.editor.valueSpan.textContent,
    aValue, aDesc + ": value span.");
}
