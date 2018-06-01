/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that class states are preserved when switching to other nodes

add_task(async function() {
  await addTab("data:text/html;charset=utf-8,<body class='class1 class2 class3'><div>");
  const {inspector, view} = await openRuleView();

  info("Open the class panel");
  view.showClassPanel();

  info("With the <body> selected, uncheck class2 and class3 in the panel");
  await toggleClassPanelCheckBox(view, "class2");
  await toggleClassPanelCheckBox(view, "class3");

  info("Now select the <div> so the panel gets refreshed");
  await selectNode("div", inspector);
  is(view.classPanel.querySelectorAll("[type=checkbox]").length, 0,
     "The panel content doesn't contain any checkboxes anymore");

  info("Select the <body> again");
  await selectNode("body", inspector);
  const checkBoxes = view.classPanel.querySelectorAll("[type=checkbox]");

  is(checkBoxes[0].dataset.name, "class1", "The first checkbox is class1");
  is(checkBoxes[0].checked, true, "The first checkbox is still checked");

  is(checkBoxes[1].dataset.name, "class2", "The second checkbox is class2");
  is(checkBoxes[1].checked, false, "The second checkbox is still unchecked");

  is(checkBoxes[2].dataset.name, "class3", "The third checkbox is class3");
  is(checkBoxes[2].checked, false, "The third checkbox is still unchecked");
});
