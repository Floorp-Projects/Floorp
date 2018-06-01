/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that keyboard and mouse navigation updates aria-active and focus
// of elements.

const TEST_URI = `
  <style>
  div { position: absolute; top: 42px; left: 42px;
  height: 100.111px; width: 100px; border: 10px solid black;
  padding: 20px; margin: 30px auto;}
  </style><div></div>
`;

add_task(async function() {
  await addTab("data:text/html," + encodeURIComponent(TEST_URI));
  const {inspector, boxmodel} = await openLayoutView();
  await selectNode("div", inspector);

  await testInitialFocus(inspector, boxmodel);
  await testChangingLevels(inspector, boxmodel);
  await testTabbingWrapAround(inspector, boxmodel);
  await testChangingLevelsByClicking(inspector, boxmodel);
});

function testInitialFocus(inspector, boxmodel) {
  info("Test that the focus is(on margin layout.");
  const doc = boxmodel.document;
  const container = doc.querySelector(".boxmodel-container");
  container.focus();
  EventUtils.synthesizeKey("KEY_Enter");

  is(container.getAttribute("activedescendant"), "boxmodel-main devtools-monospace",
    "Should be set to the position layout.");
}

function testChangingLevels(inspector, boxmodel) {
  info("Test that using arrow keys updates level.");
  const doc = boxmodel.document;
  const container = doc.querySelector(".boxmodel-container");
  container.focus();
  EventUtils.synthesizeKey("KEY_Enter");
  EventUtils.synthesizeKey("KEY_Escape");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(container.getAttribute("activedescendant"), "boxmodel-margins",
    "Should be set to the margin layout.");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(container.getAttribute("activedescendant"), "boxmodel-borders",
    "Should be set to the border layout.");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(container.getAttribute("activedescendant"), "boxmodel-paddings",
    "Should be set to the padding layout.");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(container.getAttribute("activedescendant"), "boxmodel-contents",
    "Should be set to the content layout.");

  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(container.getAttribute("activedescendant"), "boxmodel-paddings",
    "Should be set to the padding layout.");

  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(container.getAttribute("activedescendant"), "boxmodel-borders",
    "Should be set to the border layout.");

  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(container.getAttribute("activedescendant"), "boxmodel-margins",
    "Should be set to the margin layout.");

  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(container.getAttribute("activedescendant"), "boxmodel-main devtools-monospace",
    "Should be set to the position layout.");
}

function testTabbingWrapAround(inspector, boxmodel) {
  info("Test that using arrow keys updates level.");
  const doc = boxmodel.document;
  const container = doc.querySelector(".boxmodel-container");
  container.focus();
  EventUtils.synthesizeKey("KEY_Enter");

  const editLevel = container.getAttribute("activedescendant").split(" ")[0];
  const dataLevel = doc.querySelector(`.${editLevel}`).getAttribute("data-box");
  const editBoxes = [...doc.querySelectorAll(
    `[data-box="${dataLevel}"].boxmodel-editable`)];

  EventUtils.synthesizeKey("KEY_Escape");
  editBoxes[3].focus();
  EventUtils.synthesizeKey("KEY_Tab");
  is(editBoxes[0], doc.activeElement, "Top edit box should have focus.");

  editBoxes[0].focus();
  EventUtils.synthesizeKey("KEY_Tab", {shiftKey: true});
  is(editBoxes[3], doc.activeElement, "Left edit box should have focus.");
}

function testChangingLevelsByClicking(inspector, boxmodel) {
  info("Test that clicking on levels updates level.");
  const doc = boxmodel.document;
  const container = doc.querySelector(".boxmodel-container");
  container.focus();

  const marginLayout = doc.querySelector(".boxmodel-margins");
  const borderLayout = doc.querySelector(".boxmodel-borders");
  const paddingLayout = doc.querySelector(".boxmodel-paddings");
  const contentLayout = doc.querySelector(".boxmodel-contents");
  const layouts = [contentLayout, paddingLayout, borderLayout, marginLayout];

  layouts.forEach(layout => {
    layout.click();
    is(container.getAttribute("activedescendant"), layout.className,
      "Should be set to" + layout.getAttribute("data-box") + "layout.");
  });
}
