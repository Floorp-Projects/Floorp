/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let win;

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", true]] });
  win = await BrowserTestUtils.openNewBrowserWindow();
});

registerCleanupFunction(async () => {
  await SpecialPowers.popPrefEnv();
  await BrowserTestUtils.closeWindow(win);
});

function isActiveElement(el) {
  return el.getRootNode().activeElement == el;
}

add_task(async function test_keyboard_navigation() {
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  const toolButtons = await TestUtils.waitForCondition(
    () => sidebar.toolButtons,
    "Tool buttons are shown."
  );

  toolButtons[0].focus();
  ok(isActiveElement(toolButtons[0]), "First tool button is focused.");

  info("Press Arrow Down key.");
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  ok(isActiveElement(toolButtons[1]), "Second tool button is focused.");

  info("Press Arrow Up key.");
  EventUtils.synthesizeKey("KEY_ArrowUp", {}, win);
  ok(isActiveElement(toolButtons[0]), "First tool button is focused.");

  info("Press Enter key.");
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await sidebar.updateComplete;
  ok(sidebar.open, "Sidebar is open.");
  is(
    sidebar.selectedView,
    toolButtons[0].getAttribute("view"),
    "Sidebar is showing the first tool."
  );
  is(
    toolButtons[0].getAttribute("aria-pressed"),
    "true",
    "aria-pressed is true for the active tool button."
  );
  is(
    toolButtons[1].getAttribute("aria-pressed"),
    "false",
    "aria-pressed is false for the inactive tool button."
  );

  info("Press Enter key again.");
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await sidebar.updateComplete;
  ok(!sidebar.open, "Sidebar is closed.");
  is(
    toolButtons[0].getAttribute("aria-pressed"),
    "false",
    "Tool is no longer active, aria-pressed becomes false."
  );

  const customizeButton = sidebar.customizeButton;
  toolButtons[0].focus();

  info("Press Tab key.");
  EventUtils.synthesizeKey("KEY_Tab", {}, win);
  ok(isActiveElement(customizeButton), "Customize button is focused.");
});

add_task(async function test_menu_items_labeled() {
  const { document, SidebarController } = win;
  const sidebar = document.querySelector("sidebar-main");
  const allButtons = await TestUtils.waitForCondition(
    () => sidebar.allButtons,
    "All buttons are shown."
  );

  await sidebar.updateComplete;
  for (const button of allButtons) {
    const view = button.getAttribute("view");
    ok(button.title, `${view} button has a tooltip.`);
    ok(!button.hasVisibleLabel, `Collapsed ${view} button has no label.`);
  }

  SidebarController.toggleExpanded();
  await sidebar.updateComplete;
  for (const button of allButtons) {
    const view = button.getAttribute("view");
    ok(!button.title, `${view} button does not have a tooltip.`);
    // Use waitForCondition() here because sidebar needs a chance to load
    // Fluent strings.
    await TestUtils.waitForCondition(
      () => button.hasVisibleLabel,
      `Expanded ${view} button has a label.`
    );
  }
});
