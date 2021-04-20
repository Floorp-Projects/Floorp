/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that classes can be added in the class panel

// This array contains the list of test cases. Each test case contains these properties:
// - {String} textEntered The text to be entered in the field
// - {Boolean} expectNoMutation Set to true if we shouldn't wait for a DOM mutation
// - {Array} expectedClasses The expected list of classes to be applied to the DOM and to
//   be found in the class panel
const TEST_ARRAY = [
  {
    textEntered: "",
    expectNoMutation: true,
    expectedClasses: [],
  },
  {
    textEntered: "class",
    expectedClasses: ["class"],
  },
  {
    textEntered: "class",
    expectNoMutation: true,
    expectedClasses: ["class"],
  },
  {
    textEntered: "a a a a a a a a a a",
    expectedClasses: ["class", "a"],
  },
  {
    textEntered: "class2 class3",
    expectedClasses: ["class", "a", "class2", "class3"],
  },
  {
    textEntered: "                       ",
    expectNoMutation: true,
    expectedClasses: ["class", "a", "class2", "class3"],
  },
  {
    textEntered: "          class4",
    expectedClasses: ["class", "a", "class2", "class3", "class4"],
  },
  {
    textEntered: "    \t      class5      \t \t\t             ",
    expectedClasses: ["class", "a", "class2", "class3", "class4", "class5"],
  },
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8,");
  const { inspector, view } = await openRuleView();

  info("Open the class panel");
  view.showClassPanel();

  const textField = inspector.panelDoc.querySelector(
    "#ruleview-class-panel .add-class"
  );
  ok(textField, "The input field exists in the class panel");

  textField.focus();

  let onMutation;
  for (const { textEntered, expectNoMutation, expectedClasses } of TEST_ARRAY) {
    if (!expectNoMutation) {
      onMutation = inspector.once("markupmutation");
    }

    info(`Enter the test string in the field: ${textEntered}`);
    for (const key of textEntered.split("")) {
      EventUtils.synthesizeKey(key, {}, view.styleWindow);
    }

    info("Submit the change and wait for the textfield to become empty");
    const onEmpty = waitForFieldToBeEmpty(textField);
    EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);

    if (!expectNoMutation) {
      info("Wait for the DOM to change");
      await onMutation;
    }

    await onEmpty;

    info("Check the state of the DOM node");
    const className = await getAttributeInBrowser(
      gBrowser.selectedBrowser,
      "body",
      "class"
    );
    const expectedClassName = expectedClasses.length
      ? expectedClasses.join(" ")
      : null;
    is(className, expectedClassName, "The DOM node has the right className");

    info("Check the content of the class panel");
    checkClassPanelContent(
      view,
      expectedClasses.map(name => {
        return { name, state: true };
      })
    );
  }
});

function waitForFieldToBeEmpty(textField) {
  return waitForSuccess(() => !textField.value);
}
