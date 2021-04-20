/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that class panel shows the right content when selecting various nodes.

// This array contains the list of test cases. Each test case contains these properties:
// - {String} inputClassName The className on a node
// - {Array} expectedClasses The expected list of classes in the class panel
const TEST_ARRAY = [
  {
    inputClassName: "",
    expectedClasses: [],
  },
  {
    inputClassName: "         a a a a           a a a      a   a",
    expectedClasses: ["a"],
  },
  {
    inputClassName: "c1 c2 c3 c4 c5",
    expectedClasses: ["c1", "c2", "c3", "c4", "c5"],
  },
  {
    inputClassName: "a a b b c c a a b b c c",
    expectedClasses: ["a", "b", "c"],
  },
  {
    inputClassName:
      "ajdhfkasjhdkjashdkjghaskdgkauhkbdhvliashdlghaslidghasldgliashdglhasli",
    expectedClasses: [
      "ajdhfkasjhdkjashdkjghaskdgkauhkbdhvliashdlghaslidghasldgliashdglhasli",
    ],
  },
  {
    inputClassName:
      "c0 c1 c2 c3 c4 c5 c6 c7 c8 c9 " +
      "c10 c11 c12 c13 c14 c15 c16 c17 c18 c19 " +
      "c20 c21 c22 c23 c24 c25 c26 c27 c28 c29 " +
      "c30 c31 c32 c33 c34 c35 c36 c37 c38 c39 " +
      "c40 c41 c42 c43 c44 c45 c46 c47 c48 c49",
    expectedClasses: [
      "c0",
      "c1",
      "c2",
      "c3",
      "c4",
      "c5",
      "c6",
      "c7",
      "c8",
      "c9",
      "c10",
      "c11",
      "c12",
      "c13",
      "c14",
      "c15",
      "c16",
      "c17",
      "c18",
      "c19",
      "c20",
      "c21",
      "c22",
      "c23",
      "c24",
      "c25",
      "c26",
      "c27",
      "c28",
      "c29",
      "c30",
      "c31",
      "c32",
      "c33",
      "c34",
      "c35",
      "c36",
      "c37",
      "c38",
      "c39",
      "c40",
      "c41",
      "c42",
      "c43",
      "c44",
      "c45",
      "c46",
      "c47",
      "c48",
      "c49",
    ],
  },
  {
    inputClassName: "  \n  \n class1  \t   class2 \t\tclass3\t",
    expectedClasses: ["class1", "class2", "class3"],
  },
];

add_task(async function() {
  const tab = await addTab("data:text/html;charset=utf-8,<div>");
  const { inspector, view } = await openRuleView();

  await selectNode("div", inspector);

  info("Open the class panel");
  view.showClassPanel();

  for (const { inputClassName, expectedClasses } of TEST_ARRAY) {
    info(`Apply the '${inputClassName}' className to the node`);
    const onMutation = inspector.once("markupmutation");
    await setAttributeInBrowser(
      tab.linkedBrowser,
      "div",
      "class",
      inputClassName
    );
    await onMutation;

    info("Check the content of the class panel");
    checkClassPanelContent(
      view,
      expectedClasses.map(name => {
        return { name, state: true };
      })
    );
  }
});
