"use strict";

// Check that Inspect Element works in Responsive Design Mode.

const TEST_URI = `${URL_ROOT}doc_contextmenu_inspect.html`;

addRDMTask(TEST_URI, async function({ ui, manager }) {
  info("Open the responsive design mode and set its size to 500x500 to start");
  await setViewportSize(ui, manager, 500, 500);

  info("Open the inspector, rule-view and select the test node");
  const { inspector } = await openRuleView();

  const startNodeFront = inspector.selection.nodeFront;
  is(startNodeFront.displayName, "body", "body element is selected by default");

  const onSelected = inspector.once("inspector-updated");

  const contentAreaContextMenu = document.querySelector(
    "#contentAreaContextMenu"
  );
  const contextOpened = once(contentAreaContextMenu, "popupshown");

  info("Simulate a context menu event from the top browser.");
  BrowserTestUtils.synthesizeMouse(
    ui.getViewportBrowser(),
    250,
    100,
    {
      type: "contextmenu",
      button: 2,
    },
    ui.tab.linkedBrowser
  );

  await contextOpened;

  info("Triggering the inspect action");
  await gContextMenu.inspectNode();

  info("Hiding the menu");
  const contextClosed = once(contentAreaContextMenu, "popuphidden");
  contentAreaContextMenu.hidePopup();
  await contextClosed;

  await onSelected;
  const newNodeFront = inspector.selection.nodeFront;
  is(
    newNodeFront.displayName,
    "div",
    "div element is selected after using Inspect Element"
  );

  await closeToolbox();
});
