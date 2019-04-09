/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test the focus behavior when opening PanelViews.
 */

const {PanelMultiView} = ChromeUtils.import("resource:///modules/PanelMultiView.jsm");

let gAnchor;
let gPanel;
let gMainView;
let gMainButton;

add_task(async function setup() {
  let navBar = document.getElementById("nav-bar");
  gAnchor = document.createXULElement("toolbarbutton");
  // Must be focusable in order for key presses to work.
  gAnchor.style["-moz-user-focus"] = "normal";
  navBar.appendChild(gAnchor);
  gPanel = document.createXULElement("panel");
  navBar.appendChild(gPanel);
  let panelMultiView = document.createXULElement("panelmultiview");
  panelMultiView.setAttribute("mainViewId", "testMainView");
  gPanel.appendChild(panelMultiView);
  gMainView = document.createXULElement("panelview");
  gMainView.id = "testMainView";
  panelMultiView.appendChild(gMainView);
  gMainButton = document.createXULElement("button");
  gMainView.appendChild(gMainButton);

  let onPress = event => PanelMultiView.openPopup(gPanel, gAnchor, {
    triggerEvent: event,
  });
  gAnchor.addEventListener("keypress", onPress);
  gAnchor.addEventListener("click", onPress);

  registerCleanupFunction(() => {
    gAnchor.remove();
    gPanel.remove();
  });
});

// Activate the main view by pressing a key. Focus should be moved inside.
add_task(async function testMainViewByKeypress() {
  gAnchor.focus();
  await gCUITestUtils.openPanelMultiView(gPanel, gMainView,
    () => EventUtils.synthesizeKey(" "));
  Assert.equal(document.activeElement, gMainButton,
    "Focus on button in main view");
  await gCUITestUtils.hidePanelMultiView(gPanel,
    () => PanelMultiView.hidePopup(gPanel));
});

// Activate the main view by clicking the mouse. Focus should not be moved
// inside.
add_task(async function testMainViewByClick() {
  await gCUITestUtils.openPanelMultiView(gPanel, gMainView,
    () => gAnchor.click());
  Assert.notEqual(document.activeElement, gMainButton,
    "Focus not on button in main view");
  await gCUITestUtils.hidePanelMultiView(gPanel,
    () => PanelMultiView.hidePopup(gPanel));
});
