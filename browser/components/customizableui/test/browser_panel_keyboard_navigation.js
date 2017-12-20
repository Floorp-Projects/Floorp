"use strict";

/**
 * Test keyboard navigation in the app menu panel.
 */

const {PanelMultiView} = Cu.import("resource:///modules/PanelMultiView.jsm", {});
const kHelpButtonId = "appMenu-help-button";
let gHelperInstance;

add_task(async function setup() {
  gHelperInstance = new PanelMultiView(PanelUI.panel, true);
});

add_task(async function testUpDownKeys() {
  let promise = promisePanelShown(window);
  PanelUI.show();
  await promise;

  let buttons = gHelperInstance._getNavigableElements(PanelUI.mainView);

  for (let button of buttons) {
    if (button.disabled)
      continue;
    EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });
    Assert.equal(document.commandDispatcher.focusedElement, button,
      "The correct button should be focused after navigating downward");
  }

  EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });
  Assert.equal(document.commandDispatcher.focusedElement, buttons[0],
    "Pressing upwards should cycle around and select the first button again");

  for (let i = buttons.length - 1; i >= 0; --i) {
    let button = buttons[i];
    if (button.disabled)
      continue;
    EventUtils.synthesizeKey("KEY_ArrowUp", { code: "ArrowUp" });
    Assert.equal(document.commandDispatcher.focusedElement, button,
      "The first button should be focused after navigating upward");
  }

  promise = promisePanelHidden(window);
  PanelUI.hide();
  await promise;
});

add_task(async function testEnterKeyBehaviors() {
  let promise = promisePanelShown(window);
  PanelUI.show();
  await promise;

  let buttons = gHelperInstance._getNavigableElements(PanelUI.mainView);

  // Navigate to the 'Help' button, which points to a subview.
  EventUtils.synthesizeKey("KEY_ArrowUp", { code: "ArrowUp" });
  let focusedElement = document.commandDispatcher.focusedElement;
  Assert.equal(focusedElement, buttons[buttons.length - 1],
    "The last button should be focused after navigating upward");

  promise = BrowserTestUtils.waitForEvent(PanelUI.helpView, "ViewShown");
  // Make sure the Help button is in focus.
  while (!focusedElement || !focusedElement.id || focusedElement.id != kHelpButtonId) {
    EventUtils.synthesizeKey("KEY_ArrowUp", { code: "ArrowUp" });
    focusedElement = document.commandDispatcher.focusedElement;
  }
  EventUtils.synthesizeKey("VK_RETURN", { code: "Enter" });
  await promise;

  let helpButtons = gHelperInstance._getNavigableElements(PanelUI.helpView);
  Assert.ok(helpButtons[0].classList.contains("subviewbutton-back"),
    "First button in help view should be a back button");

  // For posterity, check navigating the subview using up/ down arrow keys as well.
  for (let i = helpButtons.length - 1; i >= 0; --i) {
    let button = helpButtons[i];
    if (button.disabled)
      continue;
    EventUtils.synthesizeKey("KEY_ArrowUp", { code: "ArrowUp" });
    focusedElement = document.commandDispatcher.focusedElement;
    Assert.equal(focusedElement, button, "The first button should be focused after navigating upward");
  }

  // Make sure the back button is in focus again.
  while (focusedElement != helpButtons[0]) {
    EventUtils.synthesizeKey("KEY_ArrowDown", { code: "ArrowDown" });
    focusedElement = document.commandDispatcher.focusedElement;
  }

  // The first button is the back button. Hittin Enter should navigate us back.
  promise = BrowserTestUtils.waitForEvent(PanelUI.mainView, "ViewShown");
  EventUtils.synthesizeKey("VK_RETURN", { code: "Enter" });
  await promise;

  // Let's test a 'normal' command button.
  focusedElement = document.commandDispatcher.focusedElement;
  const kFindButtonId = "appMenu-find-button";
  while (!focusedElement || !focusedElement.id || focusedElement.id != kFindButtonId) {
    EventUtils.synthesizeKey("KEY_ArrowUp", { code: "ArrowUp" });
    focusedElement = document.commandDispatcher.focusedElement;
  }
  Assert.equal(focusedElement.id, kFindButtonId, "Find button should be selected");

  promise = promisePanelHidden(window);
  EventUtils.synthesizeKey("VK_RETURN", { code: "Enter" });
  await promise;

  Assert.ok(!gFindBar.hidden, "Findbar should have opened");
  gFindBar.close();
});

add_task(async function testLeftRightKeys() {
  let promise = promisePanelShown(window);
  PanelUI.show();
  await promise;

  // Navigate to the 'Help' button, which points to a subview.
  let focusedElement = document.commandDispatcher.focusedElement;
  while (!focusedElement || !focusedElement.id || focusedElement.id != kHelpButtonId) {
    EventUtils.synthesizeKey("KEY_ArrowUp", { code: "ArrowUp" });
    focusedElement = document.commandDispatcher.focusedElement;
  }
  Assert.equal(focusedElement.id, kHelpButtonId, "The last button should be focused after navigating upward");

  // Hitting ArrowRight on a button that points to a subview should navigate us
  // there.
  promise = BrowserTestUtils.waitForEvent(PanelUI.helpView, "ViewShown");
  EventUtils.synthesizeKey("KEY_ArrowRight", { code: "ArrowRight" });
  await promise;

  // Hitting ArrowLeft should navigate us back.
  promise = BrowserTestUtils.waitForEvent(PanelUI.mainView, "ViewShown");
  EventUtils.synthesizeKey("KEY_ArrowLeft", { code: "ArrowLeft" });
  await promise;

  focusedElement = document.commandDispatcher.focusedElement;
  Assert.equal(focusedElement.id, kHelpButtonId,
               "Help button should be focused again now that we're back in the main view");

  promise = promisePanelHidden(window);
  PanelUI.hide();
  await promise;
});

add_task(async function testTabKey() {
  let promise = promisePanelShown(window);
  PanelUI.show();
  await promise;

  let buttons = gHelperInstance._getNavigableElements(PanelUI.mainView);

  for (let button of buttons) {
    if (button.disabled)
      continue;
    EventUtils.synthesizeKey("KEY_Tab", { code: "Tab" });
    Assert.equal(document.commandDispatcher.focusedElement, button,
      "The correct button should be focused after tabbing");
  }

  EventUtils.synthesizeKey("KEY_Tab", { code: "Tab" });
  Assert.equal(document.commandDispatcher.focusedElement, buttons[0],
    "Pressing tab should cycle around and select the first button again");

  for (let i = buttons.length - 1; i >= 0; --i) {
    let button = buttons[i];
    if (button.disabled)
      continue;
    EventUtils.synthesizeKey("Tab", { code: "Tab", shiftKey: true });
    Assert.equal(document.commandDispatcher.focusedElement, button,
      "The correct button should be focused after shift + tabbing");
  }

  EventUtils.synthesizeKey("KEY_Tab", { code: "Tab", shiftKey: true });
  Assert.equal(document.commandDispatcher.focusedElement, buttons[buttons.length - 1],
    "Pressing shift + tab should cycle around and select the last button again");

  promise = promisePanelHidden(window);
  PanelUI.hide();
  await promise;
});
