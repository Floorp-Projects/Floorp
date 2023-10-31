/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test the focus behavior when opening PanelViews.
 */

let gAnchor;
let gPanel;
let gPanelMultiView;
let gMainView;
let gMainButton;
let gMainSubButton;
let gSubView;
let gSubButton;

function createWith(doc, tag, props) {
  let el = doc.createXULElement(tag);
  for (let prop in props) {
    el.setAttribute(prop, props[prop]);
  }
  return el;
}

add_setup(async function () {
  let navBar = document.getElementById("nav-bar");
  gAnchor = document.createXULElement("toolbarbutton");
  // Must be focusable in order for key presses to work.
  gAnchor.style["-moz-user-focus"] = "normal";
  navBar.appendChild(gAnchor);
  let onPress = event =>
    PanelMultiView.openPopup(gPanel, gAnchor, {
      triggerEvent: event,
    });
  gAnchor.addEventListener("keypress", onPress);
  gAnchor.addEventListener("click", onPress);
  gAnchor.setAttribute("aria-label", "test label");
  gPanel = document.createXULElement("panel");
  navBar.appendChild(gPanel);
  gPanelMultiView = document.createXULElement("panelmultiview");
  gPanelMultiView.setAttribute("mainViewId", "testMainView");
  gPanel.appendChild(gPanelMultiView);

  gMainView = document.createXULElement("panelview");
  gMainView.id = "testMainView";
  gPanelMultiView.appendChild(gMainView);
  gMainButton = createWith(document, "button", { label: "gMainButton" });
  gMainView.appendChild(gMainButton);
  gMainSubButton = createWith(document, "button", { label: "gMainSubButton" });
  gMainView.appendChild(gMainSubButton);
  gMainSubButton.addEventListener("command", () =>
    gPanelMultiView.showSubView("testSubView", gMainSubButton)
  );

  gSubView = document.createXULElement("panelview");
  gSubView.id = "testSubView";
  gPanelMultiView.appendChild(gSubView);
  gSubButton = createWith(document, "button", { label: "gSubButton" });
  gSubView.appendChild(gSubButton);

  registerCleanupFunction(() => {
    gAnchor.remove();
    gPanel.remove();
  });
});

// Activate the main view by pressing a key. Focus should be moved inside.
add_task(async function testMainViewByKeypress() {
  gAnchor.focus();
  await gCUITestUtils.openPanelMultiView(gPanel, gMainView, () =>
    EventUtils.synthesizeKey(" ")
  );
  Assert.equal(
    document.activeElement,
    gMainButton,
    "Focus on button in main view"
  );
  await gCUITestUtils.hidePanelMultiView(gPanel, () =>
    PanelMultiView.hidePopup(gPanel)
  );
});

// Activate the main view by clicking the mouse. Focus should not be moved
// inside.
add_task(async function testMainViewByClick() {
  await gCUITestUtils.openPanelMultiView(gPanel, gMainView, () =>
    gAnchor.click()
  );
  Assert.notEqual(
    document.activeElement,
    gMainButton,
    "Focus not on button in main view"
  );
  await gCUITestUtils.hidePanelMultiView(gPanel, () =>
    PanelMultiView.hidePopup(gPanel)
  );
});

// Activate the subview by pressing a key. Focus should be moved to the first
// button after the Back button.
add_task(async function testSubViewByKeypress() {
  await gCUITestUtils.openPanelMultiView(gPanel, gMainView, () =>
    gAnchor.click()
  );
  while (document.activeElement != gMainSubButton) {
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  }
  let shown = BrowserTestUtils.waitForEvent(gSubView, "ViewShown");
  EventUtils.synthesizeKey(" ");
  await shown;
  Assert.equal(
    document.activeElement,
    gSubButton,
    "Focus on first button after Back button in subview"
  );
  await gCUITestUtils.hidePanelMultiView(gPanel, () =>
    PanelMultiView.hidePopup(gPanel)
  );
});

// Activate the subview by clicking the mouse. Focus should not be moved
// inside.
add_task(async function testSubViewByClick() {
  await gCUITestUtils.openPanelMultiView(gPanel, gMainView, () =>
    gAnchor.click()
  );
  let shown = BrowserTestUtils.waitForEvent(gSubView, "ViewShown");
  gMainSubButton.click();
  await shown;
  let backButton = gSubView.querySelector(".subviewbutton-back");
  Assert.notEqual(
    document.activeElement,
    backButton,
    "Focus not on Back button in subview"
  );
  Assert.notEqual(
    document.activeElement,
    gSubButton,
    "Focus not on button after Back button in subview"
  );
  await gCUITestUtils.hidePanelMultiView(gPanel, () =>
    PanelMultiView.hidePopup(gPanel)
  );
});

// Test that focus is restored when going back to a previous view.
add_task(async function testBackRestoresFocus() {
  await gCUITestUtils.openPanelMultiView(gPanel, gMainView, () =>
    gAnchor.click()
  );
  while (document.activeElement != gMainSubButton) {
    EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  }
  let shown = BrowserTestUtils.waitForEvent(gSubView, "ViewShown");
  EventUtils.synthesizeKey(" ");
  await shown;
  shown = BrowserTestUtils.waitForEvent(gMainView, "ViewShown");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  await shown;
  Assert.equal(
    document.activeElement,
    gMainSubButton,
    "Focus on sub button in main view"
  );
  await gCUITestUtils.hidePanelMultiView(gPanel, () =>
    PanelMultiView.hidePopup(gPanel)
  );
});
