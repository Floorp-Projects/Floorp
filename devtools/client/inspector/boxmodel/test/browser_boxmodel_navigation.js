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
  const { inspector, boxmodel } = await openLayoutView();
  await selectNode("div", inspector);

  await testInitialFocus(inspector, boxmodel);
  await testChangingLevels(inspector, boxmodel);
  await testTabbingThroughItems(inspector, boxmodel);
  await testChangingLevelsByClicking(inspector, boxmodel);
});

function testInitialFocus(inspector, boxmodel) {
  info("Test that the focus is(on margin layout.");
  const doc = boxmodel.document;
  const container = doc.querySelector(".boxmodel-container");
  container.focus();
  EventUtils.synthesizeKey("KEY_Enter");

  is(
    container.dataset.activeDescendantClassName,
    "boxmodel-main devtools-monospace",
    "Should be set to the position layout."
  );
}

function testChangingLevels(inspector, boxmodel) {
  info("Test that using arrow keys updates level.");
  const doc = boxmodel.document;
  const container = doc.querySelector(".boxmodel-container");
  container.focus();
  EventUtils.synthesizeKey("KEY_Enter");
  EventUtils.synthesizeKey("KEY_Escape");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(
    container.dataset.activeDescendantClassName,
    "boxmodel-margins",
    "Should be set to the margin layout."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(
    container.dataset.activeDescendantClassName,
    "boxmodel-borders",
    "Should be set to the border layout."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(
    container.dataset.activeDescendantClassName,
    "boxmodel-paddings",
    "Should be set to the padding layout."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(
    container.dataset.activeDescendantClassName,
    "boxmodel-contents",
    "Should be set to the content layout."
  );

  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(
    container.dataset.activeDescendantClassName,
    "boxmodel-paddings",
    "Should be set to the padding layout."
  );

  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(
    container.dataset.activeDescendantClassName,
    "boxmodel-borders",
    "Should be set to the border layout."
  );

  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(
    container.dataset.activeDescendantClassName,
    "boxmodel-margins",
    "Should be set to the margin layout."
  );

  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(
    container.dataset.activeDescendantClassName,
    "boxmodel-main devtools-monospace",
    "Should be set to the position layout."
  );
}

function testTabbingThroughItems(inspector, boxmodel) {
  info("Test that using Tab key moves focus to next/previous input field.");
  const doc = boxmodel.document;
  const container = doc.querySelector(".boxmodel-container");
  container.focus();
  EventUtils.synthesizeKey("KEY_Enter");

  const editBoxes = [...doc.querySelectorAll("[data-box].boxmodel-editable")];

  const editBoxesInfo = [
    { name: "position-top", itemId: "position-top-id" },
    { name: "position-right", itemId: "position-right-id" },
    { name: "position-bottom", itemId: "position-bottom-id" },
    { name: "position-left", itemId: "position-left-id" },
    { name: "margin-top", itemId: "margin-top-id" },
    { name: "margin-right", itemId: "margin-right-id" },
    { name: "margin-bottom", itemId: "margin-bottom-id" },
    { name: "margin-left", itemId: "margin-left-id" },
    { name: "border-top-width", itemId: "border-top-width-id" },
    { name: "border-right-width", itemId: "border-right-width-id" },
    { name: "border-bottom-width", itemId: "border-bottom-width-id" },
    { name: "border-left-width", itemId: "border-left-width-id" },
    { name: "padding-top", itemId: "padding-top-id" },
    { name: "padding-right", itemId: "padding-right-id" },
    { name: "padding-bottom", itemId: "padding-bottom-id" },
    { name: "padding-left", itemId: "padding-left-id" },
    { name: "width", itemId: "width-id" },
    { name: "height", itemId: "height-id" },
  ];

  // Check whether tabbing through box model items works
  // Note that the test checks whether wrapping around the box model works
  // by letting the loop run beyond the number of indexes to start with
  // the first item again.
  for (let i = 0; i <= editBoxesInfo.length; i++) {
    const itemIndex = i % editBoxesInfo.length;
    const editBoxInfo = editBoxesInfo[itemIndex];
    is(
      editBoxes[itemIndex].parentElement.id,
      editBoxInfo.itemId,
      `${editBoxInfo.name} item is current`
    );
    is(
      editBoxes[itemIndex].previousElementSibling?.localName,
      "input",
      `Input shown for ${editBoxInfo.name} item`
    );

    // Pressing Tab should not be synthesized for the last item to
    // wrap to the very last item again when tabbing in reversed order.
    if (i < editBoxesInfo.length) {
      EventUtils.synthesizeKey("KEY_Tab");
    }
  }

  // Check whether reversed tabbing through box model items works
  for (let i = editBoxesInfo.length; i >= 0; i--) {
    const itemIndex = i % editBoxesInfo.length;
    const editBoxInfo = editBoxesInfo[itemIndex];
    is(
      editBoxes[itemIndex].parentElement.id,
      editBoxInfo.itemId,
      `${editBoxInfo.name} item is current`
    );
    is(
      editBoxes[itemIndex].previousElementSibling?.localName,
      "input",
      `Input shown for ${editBoxInfo.name} item`
    );
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  }
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
    is(
      container.dataset.activeDescendantClassName,
      layout.className,
      `Should be set to ${layout.getAttribute("data-box")} layout.`
    );
  });
}
