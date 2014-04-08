/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changing the current element's attributes and style refreshes the
// rule-view

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8,browser_ruleview_update.js");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Creating the test document");
  let style = '' +
    '#testid {' +
    '  background-color: blue;' +
    '} ' +
    '.testclass {' +
    '  background-color: green;' +
    '}';
  let styleNode = addStyle(content.document, style);
  content.document.body.innerHTML = '<div id="testid" class="testclass">Styled Node</div>';

  info("Getting the test node");
  let testElement = getNode("#testid");

  info("Adding inline style");
  let elementStyle = 'margin-top: 1px; padding-top: 5px;'
  testElement.setAttribute("style", elementStyle);

  info("Selecting the test node")
  yield selectNode(testElement, inspector);

  info("Test that changing the element's attributes refreshes the rule-view");
  yield testRuleChanges(inspector, view, testElement);
  yield testRuleChange1(inspector, view, testElement);
  yield testRuleChange2(inspector, view, testElement);

  yield testPropertyChanges(inspector, view, testElement);
  yield testPropertyChange0(inspector, view, testElement);
  yield testPropertyChange1(inspector, view, testElement);
  yield testPropertyChange2(inspector, view, testElement);
  yield testPropertyChange3(inspector, view, testElement);
  yield testPropertyChange4(inspector, view, testElement);
  yield testPropertyChange5(inspector, view, testElement);
  yield testPropertyChange6(inspector, view, testElement);
  yield testPropertyChange7(inspector, view, testElement);
});

function* testRuleChanges(inspector, ruleView, testElement) {
  let selectors = ruleView.doc.querySelectorAll(".ruleview-selector");
  is(selectors.length, 3, "Three rules visible.");
  is(selectors[0].textContent.indexOf("element"), 0, "First item is inline style.");
  is(selectors[1].textContent.indexOf("#testid"), 0, "Second item is id rule.");
  is(selectors[2].textContent.indexOf(".testclass"), 0, "Third item is class rule.");

  // Change the id and refresh.
  let ruleViewRefreshed = inspector.once("rule-view-refreshed");
  testElement.setAttribute("id", "differentid");
  yield ruleViewRefreshed;
}

function* testRuleChange1(inspector, ruleView, testElement) {
  let selectors = ruleView.doc.querySelectorAll(".ruleview-selector");
  is(selectors.length, 2, "Two rules visible.");
  is(selectors[0].textContent.indexOf("element"), 0, "First item is inline style.");
  is(selectors[1].textContent.indexOf(".testclass"), 0, "Second item is class rule.");

  let ruleViewRefreshed = inspector.once("rule-view-refreshed");
  testElement.setAttribute("id", "testid");
  yield ruleViewRefreshed;
}

function* testRuleChange2(inspector, ruleView, testElement) {
  let selectors = ruleView.doc.querySelectorAll(".ruleview-selector");
  is(selectors.length, 3, "Three rules visible.");
  is(selectors[0].textContent.indexOf("element"), 0, "First item is inline style.");
  is(selectors[1].textContent.indexOf("#testid"), 0, "Second item is id rule.");
  is(selectors[2].textContent.indexOf(".testclass"), 0, "Third item is class rule.");
}

function* testPropertyChanges(inspector, ruleView, testElement) {
  let rule = ruleView._elementStyle.rules[0];
  let ruleEditor = ruleView._elementStyle.rules[0].editor;

  let onRefreshed = inspector.once("rule-view-refreshed");
  // Add a second margin-top value, just to make things interesting.
  ruleEditor.addProperty("margin-top", "5px", "");
  yield onRefreshed;
}

function* testPropertyChange0(inspector, ruleView, testElement) {
  let rule = ruleView._elementStyle.rules[0];
  validateTextProp(rule.textProps[0], false, "margin-top", "1px", "Original margin property active");

  let onRefreshed = inspector.once("rule-view-refreshed");
  testElement.setAttribute("style", "margin-top: 1px; padding-top: 5px");
  yield onRefreshed;
}

function* testPropertyChange1(inspector, ruleView, testElement) {
  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[0], true, "margin-top", "1px", "First margin property re-enabled");
  validateTextProp(rule.textProps[2], false, "margin-top", "5px", "Second margin property disabled");

  let onRefreshed = inspector.once("rule-view-refreshed");
  // Now set it back to 5px, the 5px value should be re-enabled.
  testElement.setAttribute("style", "margin-top: 5px; padding-top: 5px;");
  yield onRefreshed;
}

function* testPropertyChange2(inspector, ruleView, testElement) {
  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[0], false, "margin-top", "1px", "First margin property re-enabled");
  validateTextProp(rule.textProps[2], true, "margin-top", "5px", "Second margin property disabled");

  let onRefreshed = inspector.once("rule-view-refreshed");
  // Set the margin property to a value that doesn't exist in the editor.
  // Should reuse the currently-enabled element (the second one.)
  testElement.setAttribute("style", "margin-top: 15px; padding-top: 5px;");
  yield onRefreshed;
}

function* testPropertyChange3(inspector, ruleView, testElement) {
  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[0], false, "margin-top", "1px", "First margin property re-enabled");
  validateTextProp(rule.textProps[2], true, "margin-top", "15px", "Second margin property disabled");

  let onRefreshed = inspector.once("rule-view-refreshed");
  // Remove the padding-top attribute.  Should disable the padding property but not remove it.
  testElement.setAttribute("style", "margin-top: 5px;");
  yield onRefreshed;
}

function* testPropertyChange4(inspector, ruleView, testElement) {
  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[1], false, "padding-top", "5px", "Padding property disabled");

  let onRefreshed = inspector.once("rule-view-refreshed");
  // Put the padding-top attribute back in, should re-enable the padding property.
  testElement.setAttribute("style", "margin-top: 5px; padding-top: 25px");
  yield onRefreshed;
}

function* testPropertyChange5(inspector, ruleView, testElement) {
  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3, "Correct number of properties");
  validateTextProp(rule.textProps[1], true, "padding-top", "25px", "Padding property enabled");

  let onRefreshed = inspector.once("rule-view-refreshed");
  // Add an entirely new property
  testElement.setAttribute("style", "margin-top: 5px; padding-top: 25px; padding-left: 20px;");
  yield onRefreshed;
}

function* testPropertyChange6(inspector, ruleView, testElement) {
  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 4, "Added a property");
  validateTextProp(rule.textProps[3], true, "padding-left", "20px", "Padding property enabled");

  let onRefreshed = inspector.once("rule-view-refreshed");
  // Add an entirely new property
  testElement.setAttribute("style", "background: url(\"chrome://branding/content/about-logo.png\") repeat scroll 0% 0% red");
  yield onRefreshed;
}

function* testPropertyChange7(inspector, ruleView, testElement) {
  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 5, "Added a property");
  validateTextProp(rule.textProps[4], true, "background",
                   "url(\"chrome://branding/content/about-logo.png\") repeat scroll 0% 0% red",
                   "shortcut property correctly set",
                   "url('chrome://branding/content/about-logo.png') repeat scroll 0% 0% #F00");
}

function validateTextProp(aProp, aEnabled, aName, aValue, aDesc, valueSpanText) {
  is(aProp.enabled, aEnabled, aDesc + ": enabled.");
  is(aProp.name, aName, aDesc + ": name.");
  is(aProp.value, aValue, aDesc + ": value.");

  is(aProp.editor.enable.hasAttribute("checked"), aEnabled, aDesc + ": enabled checkbox.");
  is(aProp.editor.nameSpan.textContent, aName, aDesc + ": name span.");
  is(aProp.editor.valueSpan.textContent, valueSpanText || aValue, aDesc + ": value span.");
}
