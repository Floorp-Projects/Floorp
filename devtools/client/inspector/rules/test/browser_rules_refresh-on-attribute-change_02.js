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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view, testActor} = await openRuleView();
  await selectNode("#testid", inspector);

  await testPropertyChanges(inspector, view);
  await testPropertyChange0(inspector, view, "#testid", testActor);
  await testPropertyChange1(inspector, view, "#testid", testActor);
  await testPropertyChange2(inspector, view, "#testid", testActor);
  await testPropertyChange3(inspector, view, "#testid", testActor);
  await testPropertyChange4(inspector, view, "#testid", testActor);
  await testPropertyChange5(inspector, view, "#testid", testActor);
  await testPropertyChange6(inspector, view, "#testid", testActor);
});

async function testPropertyChanges(inspector, ruleView) {
  info("Adding a second margin-top value in the element selector");
  let ruleEditor = ruleView._elementStyle.rules[0].editor;
  let onRefreshed = inspector.once("rule-view-refreshed");
  ruleEditor.addProperty("margin-top", "5px", "", true);
  await onRefreshed;

  let rule = ruleView._elementStyle.rules[0];
  validateTextProp(rule.textProps[0], false, "margin-top", "1px",
    "Original margin property active");
}

async function testPropertyChange0(inspector, ruleView, selector, testActor) {
  await changeElementStyle(selector, "margin-top: 1px; padding-top: 5px",
    inspector, testActor);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3,
    "Correct number of properties");
  validateTextProp(rule.textProps[0], true, "margin-top", "1px",
    "First margin property re-enabled");
  validateTextProp(rule.textProps[2], false, "margin-top", "5px",
    "Second margin property disabled");
}

async function testPropertyChange1(inspector, ruleView, selector, testActor) {
  info("Now set it back to 5px, the 5px value should be re-enabled.");
  await changeElementStyle(selector, "margin-top: 5px; padding-top: 5px;",
    inspector, testActor);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3,
    "Correct number of properties");
  validateTextProp(rule.textProps[0], false, "margin-top", "1px",
    "First margin property re-enabled");
  validateTextProp(rule.textProps[2], true, "margin-top", "5px",
    "Second margin property disabled");
}

async function testPropertyChange2(inspector, ruleView, selector, testActor) {
  info("Set the margin property to a value that doesn't exist in the editor.");
  info("Should reuse the currently-enabled element (the second one.)");
  await changeElementStyle(selector, "margin-top: 15px; padding-top: 5px;",
    inspector, testActor);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3,
    "Correct number of properties");
  validateTextProp(rule.textProps[0], false, "margin-top", "1px",
    "First margin property re-enabled");
  validateTextProp(rule.textProps[2], true, "margin-top", "15px",
    "Second margin property disabled");
}

async function testPropertyChange3(inspector, ruleView, selector, testActor) {
  info("Remove the padding-top attribute. Should disable the padding " +
    "property but not remove it.");
  await changeElementStyle(selector, "margin-top: 5px;", inspector, testActor);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3,
    "Correct number of properties");
  validateTextProp(rule.textProps[1], false, "padding-top", "5px",
    "Padding property disabled");
}

async function testPropertyChange4(inspector, ruleView, selector, testActor) {
  info("Put the padding-top attribute back in, should re-enable the " +
    "padding property.");
  await changeElementStyle(selector, "margin-top: 5px; padding-top: 25px",
    inspector, testActor);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 3,
    "Correct number of properties");
  validateTextProp(rule.textProps[1], true, "padding-top", "25px",
    "Padding property enabled");
}

async function testPropertyChange5(inspector, ruleView, selector, testActor) {
  info("Add an entirely new property");
  await changeElementStyle(selector,
    "margin-top: 5px; padding-top: 25px; padding-left: 20px;",
    inspector, testActor);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 4,
    "Added a property");
  validateTextProp(rule.textProps[3], true, "padding-left", "20px",
    "Padding property enabled");
}

async function testPropertyChange6(inspector, ruleView, selector, testActor) {
  info("Add an entirely new property again");
  await changeElementStyle(selector, "background: red " +
    "url(\"chrome://branding/content/about-logo.png\") repeat scroll 0% 0%",
    inspector, testActor);

  let rule = ruleView._elementStyle.rules[0];
  is(rule.editor.element.querySelectorAll(".ruleview-property").length, 5,
    "Added a property");
  validateTextProp(rule.textProps[4], true, "background",
                   "red url(\"chrome://branding/content/about-logo.png\") repeat scroll 0% 0%",
                   "shortcut property correctly set");
}

async function changeElementStyle(selector, style, inspector, testActor) {
  let onRefreshed = inspector.once("rule-view-refreshed");
  await testActor.setAttribute(selector, "style", style);
  await onRefreshed;
}

function validateTextProp(prop, enabled, name, value, desc) {
  is(prop.enabled, enabled, desc + ": enabled.");
  is(prop.name, name, desc + ": name.");
  is(prop.value, value, desc + ": value.");

  is(prop.editor.enable.hasAttribute("checked"), enabled,
    desc + ": enabled checkbox.");
  is(prop.editor.nameSpan.textContent, name, desc + ": name span.");
  is(prop.editor.valueSpan.textContent,
    value, desc + ": value span.");
}
