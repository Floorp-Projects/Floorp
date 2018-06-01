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

// The series of test cases to run. Each case is an object with the following properties:
// - {String} desc The test case description
// - {Function} setup The setup function to execute for this case
// - {Array} properties The properties to expect as a result. Each of them is an object:
//   - {String} name The expected property name
//   - {String} value The expected property value
//   - {Boolean} overridden The expected property overridden state
//   - {Boolean} enabled The expected property enabled state
const TEST_DATA = [{
  desc: "Adding a second margin-top value in the element selector",
  setup: async function({ view }) {
    await addProperty(view, 0, "margin-top", "5px");
  },
  properties: [
    { name: "margin-top", value: "1px", overridden: true, enabled: true },
    { name: "padding-top", value: "5px", overridden: false, enabled: true },
    { name: "margin-top", value: "5px", overridden: false, enabled: true }
  ]
}, {
  desc: "Setting the element style to its original value",
  setup: async function({ inspector, testActor }) {
    await changeElementStyle("#testid", "margin-top: 1px; padding-top: 5px", inspector,
                             testActor);
  },
  properties: [
    { name: "margin-top", value: "1px", overridden: false, enabled: true },
    { name: "padding-top", value: "5px", overridden: false, enabled: true },
    { name: "margin-top", value: "5px", overridden: false, enabled: false }
  ]
}, {
  desc: "Set the margin-top back to 5px, the previous property should be re-enabled",
  setup: async function({ inspector, testActor }) {
    await changeElementStyle("#testid", "margin-top: 5px; padding-top: 5px;", inspector,
                            testActor);
  },
  properties: [
    { name: "margin-top", value: "1px", overridden: false, enabled: false },
    { name: "padding-top", value: "5px", overridden: false, enabled: true },
    { name: "margin-top", value: "5px", overridden: false, enabled: true }
  ]
}, {
  desc: "Set the margin property to a value that doesn't exist in the editor, which " +
        "should reuse the currently re-enabled property (the second one)",
  setup: async function({ inspector, testActor }) {
    await changeElementStyle("#testid", "margin-top: 15px; padding-top: 5px;", inspector,
                             testActor);
  },
  properties: [
    { name: "margin-top", value: "1px", overridden: false, enabled: false },
    { name: "padding-top", value: "5px", overridden: false, enabled: true },
    { name: "margin-top", value: "15px", overridden: false, enabled: true }
  ]
}, {
  desc: "Remove the padding-top attribute. Should disable the padding property but not " +
        "remove it",
  setup: async function({ inspector, testActor }) {
    await changeElementStyle("#testid", "margin-top: 5px;", inspector, testActor);
  },
  properties: [
    { name: "margin-top", value: "1px", overridden: false, enabled: false },
    { name: "padding-top", value: "5px", overridden: false, enabled: false },
    { name: "margin-top", value: "5px", overridden: false, enabled: true }
  ]
}, {
  desc: "Put the padding-top attribute back in, should re-enable the padding property",
  setup: async function({ inspector, testActor }) {
    await changeElementStyle("#testid", "margin-top: 5px; padding-top: 25px", inspector,
                             testActor);
  },
  properties: [
    { name: "margin-top", value: "1px", overridden: false, enabled: false },
    { name: "padding-top", value: "25px", overridden: false, enabled: true },
    { name: "margin-top", value: "5px", overridden: false, enabled: true }
  ]
}, {
  desc: "Add an entirely new property",
  setup: async function({ inspector, testActor }) {
    await changeElementStyle("#testid",
                            "margin-top: 5px; padding-top: 25px; padding-left: 20px;",
                            inspector, testActor);
  },
  properties: [
    { name: "margin-top", value: "1px", overridden: false, enabled: false },
    { name: "padding-top", value: "25px", overridden: false, enabled: true },
    { name: "margin-top", value: "5px", overridden: false, enabled: true },
    { name: "padding-left", value: "20px", overridden: false, enabled: true }
  ]
}, {
  desc: "Add an entirely new property again",
  setup: async function({ inspector, testActor }) {
    await changeElementStyle("#testid", "color: red", inspector, testActor);
  },
  properties: [
    { name: "margin-top", value: "1px", overridden: false, enabled: false },
    { name: "padding-top", value: "25px", overridden: false, enabled: false },
    { name: "margin-top", value: "5px", overridden: false, enabled: false },
    { name: "padding-left", value: "20px", overridden: false, enabled: false },
    { name: "color", value: "red", overridden: false, enabled: true }
  ]
}];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view, testActor } = await openRuleView();
  await selectNode("#testid", inspector);

  for (const { desc, setup, properties } of TEST_DATA) {
    info(desc);

    await setup({ inspector, view, testActor });

    const rule = view._elementStyle.rules[0];
    is(rule.editor.element.querySelectorAll(".ruleview-property").length,
       properties.length, "The correct number of properties was found");

    properties.forEach(({ name, value, overridden, enabled }, index) => {
      validateTextProp(rule.textProps[index], overridden, enabled, name, value);
    });
  }
});

async function changeElementStyle(selector, style, inspector, testActor) {
  info(`Setting ${selector}'s element style to ${style}`);
  const onRefreshed = inspector.once("rule-view-refreshed");
  await testActor.setAttribute(selector, "style", style);
  await onRefreshed;
}

function validateTextProp(prop, overridden, enabled, name, value) {
  is(prop.name, name, `${name} property name is correct`);
  is(prop.editor.nameSpan.textContent, name, `${name} property name is correct in UI`);

  is(prop.value, value, `${name} property value is correct`);
  is(prop.editor.valueSpan.textContent, value, `${name} property value is correct in UI`);

  is(prop.enabled, enabled, `${name} property enabled state correct`);
  is(prop.overridden, overridden, `${name} property overridden state correct`);
}
