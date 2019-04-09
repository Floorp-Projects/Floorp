/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that element hover states are not triggered when touch is enabled.

const TEST_URL = `${URL_ROOT}hover.html`;

addRDMTask(TEST_URL, async function({ ui }) {
  reloadOnTouchChange(true);

  await injectEventUtilsInContentTask(ui.getViewportBrowser());
  await toggleTouchSimulation(ui);

  info("Test element hover states when touch is enabled.");
  await testButtonHoverState(ui, "rgb(255, 0, 0)");
  await testDropDownHoverState(ui, "none");

  await toggleTouchSimulation(ui);

  info("Test element hover states when touch is disabled.");
  await testButtonHoverState(ui, "rgb(0, 0, 0)");
  await testDropDownHoverState(ui, "block");

  reloadOnTouchChange(false);
});

async function testButtonHoverState(ui, expected) {
  await ContentTask.spawn(ui.getViewportBrowser(), { expected },
    async function(args) {
      let button = content.document.querySelector("button");
      const { expected: contentExpected } = args;

      info("Move mouse into the button element.");
      await EventUtils.synthesizeMouseAtCenter(button,
            { type: "mousemove", isSynthesized: false }, content);
      button = content.document.querySelector("button");
      const win = content.document.defaultView;

      is(win.getComputedStyle(button).getPropertyValue("background-color"),
        contentExpected, `Button background color is ${contentExpected}.`);
    });
}

async function testDropDownHoverState(ui, expected) {
  await ContentTask.spawn(ui.getViewportBrowser(), { expected },
    async function(args) {
      const dropDownMenu = content.document.querySelector(".drop-down-menu");
      const { expected: contentExpected } = args;

      info("Move mouse into the drop down menu.");
      await EventUtils.synthesizeMouseAtCenter(dropDownMenu,
            { type: "mousemove", isSynthesized: false }, content);
      const win = content.document.defaultView;
      const menuItems = content.document.querySelector(".menu-items-list");

      is(win.getComputedStyle(menuItems).getPropertyValue("display"), contentExpected,
        `Menu items is display: ${contentExpected}.`);
    });
}
