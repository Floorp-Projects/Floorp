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

add_task(function* () {
  yield addTab("data:text/html," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openBoxModelView();
  yield selectNode("div", inspector);

  yield testInitialFocus(inspector, view);
  yield testChangingLevels(inspector, view);
  yield testTabbingWrapAround(inspector, view);
  yield testChangingLevelsByClicking(inspector, view);
});

function* testInitialFocus(inspector, view) {
  info("Test that the focus is on margin layout.");
  let viewdoc = view.document;
  let boxmodel = viewdoc.querySelector(".boxmodel-container");
  boxmodel.focus();
  EventUtils.synthesizeKey("VK_RETURN", {});

  is(boxmodel.getAttribute("activedescendant"), "boxmodel-main devtools-monospace",
    "Should be set to the position layout.");
}

function* testChangingLevels(inspector, view) {
  info("Test that using arrow keys updates level.");
  let viewdoc = view.document;
  let boxmodel = viewdoc.querySelector(".boxmodel-container");
  boxmodel.focus();
  EventUtils.synthesizeKey("VK_RETURN", {});
  EventUtils.synthesizeKey("VK_ESCAPE", {});

  EventUtils.synthesizeKey("VK_DOWN", {});
  is(boxmodel.getAttribute("activedescendant"), "boxmodel-margins",
    "Should be set to the margin layout.");

  EventUtils.synthesizeKey("VK_DOWN", {});
  is(boxmodel.getAttribute("activedescendant"), "boxmodel-borders",
    "Should be set to the border layout.");

  EventUtils.synthesizeKey("VK_DOWN", {});
  is(boxmodel.getAttribute("activedescendant"), "boxmodel-paddings",
    "Should be set to the padding layout.");

  EventUtils.synthesizeKey("VK_DOWN", {});
  is(boxmodel.getAttribute("activedescendant"), "boxmodel-contents",
    "Should be set to the content layout.");

  EventUtils.synthesizeKey("VK_UP", {});
  is(boxmodel.getAttribute("activedescendant"), "boxmodel-paddings",
    "Should be set to the padding layout.");

  EventUtils.synthesizeKey("VK_UP", {});
  is(boxmodel.getAttribute("activedescendant"), "boxmodel-borders",
    "Should be set to the border layout.");

  EventUtils.synthesizeKey("VK_UP", {});
  is(boxmodel.getAttribute("activedescendant"), "boxmodel-margins",
    "Should be set to the margin layout.");

  EventUtils.synthesizeKey("VK_UP", {});
  is(boxmodel.getAttribute("activedescendant"), "boxmodel-main devtools-monospace",
    "Should be set to the position layout.");
}

function* testTabbingWrapAround(inspector, view) {
  info("Test that using arrow keys updates level.");
  let viewdoc = view.document;
  let boxmodel = viewdoc.querySelector(".boxmodel-container");
  boxmodel.focus();
  EventUtils.synthesizeKey("VK_RETURN", {});

  let editLevel = boxmodel.getAttribute("activedescendant").split(" ")[0];
  let dataLevel = viewdoc.querySelector(`.${editLevel}`).getAttribute("data-box");
  let editBoxes = [...viewdoc.querySelectorAll(
    `[data-box="${dataLevel}"].boxmodel-editable`)];

  EventUtils.synthesizeKey("VK_ESCAPE", {});
  editBoxes[3].focus();
  EventUtils.synthesizeKey("VK_TAB", {});
  is(editBoxes[0], viewdoc.activeElement, "Top edit box should have focus.");

  editBoxes[0].focus();
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  is(editBoxes[3], viewdoc.activeElement, "Left edit box should have focus.");
}

function* testChangingLevelsByClicking(inspector, view) {
  info("Test that clicking on levels updates level.");
  let viewdoc = view.document;
  let boxmodel = viewdoc.querySelector(".boxmodel-container");
  boxmodel.focus();

  let marginLayout = viewdoc.querySelector(".boxmodel-margins");
  let borderLayout = viewdoc.querySelector(".boxmodel-borders");
  let paddingLayout = viewdoc.querySelector(".boxmodel-paddings");
  let contentLayout = viewdoc.querySelector(".boxmodel-contents");
  let layouts = [contentLayout, paddingLayout, borderLayout, marginLayout];

  layouts.forEach(layout => {
    layout.click();
    is(boxmodel.getAttribute("activedescendant"), layout.className,
      "Should be set to" + layout.getAttribute("data-box") + "layout.");
  });
}
