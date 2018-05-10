/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether options panel toggled by key event and "Settings" on the meatball menu.

const { Toolbox } = require("devtools/client/framework/toolbox");

add_task(async function() {
  const tab = await addTab("about:blank");
  const toolbox = await openToolboxForTab(tab, "webconsole", Toolbox.HostType.BOTTOM);

  info("Check the option panel was selected after sending F1 key event");
  await sendOptionsKeyEvent(toolbox);
  is(toolbox.currentToolId, "options", "The options panel should be selected");

  info("Check the last selected panel was selected after sending F1 key event");
  await sendOptionsKeyEvent(toolbox);
  is(toolbox.currentToolId, "webconsole", "The webconsole panel should be selected");

  info("Check the option panel was selected after clicking 'Settings' menu");
  await clickSettingsMenu(toolbox);
  is(toolbox.currentToolId, "options", "The options panel should be selected");

  info("Check the last selected panel was selected after clicking 'Settings' menu");
  await sendOptionsKeyEvent(toolbox);
  is(toolbox.currentToolId, "webconsole", "The webconsole panel should be selected");

  info("Check the combination of key event and 'Settings' menu");
  await sendOptionsKeyEvent(toolbox);
  await clickSettingsMenu(toolbox);
  is(toolbox.currentToolId, "webconsole", "The webconsole panel should be selected");
  await clickSettingsMenu(toolbox);
  await sendOptionsKeyEvent(toolbox);
  is(toolbox.currentToolId, "webconsole", "The webconsole panel should be selected");
});

async function sendOptionsKeyEvent(toolbox) {
  const onReady = toolbox.once("select");
  EventUtils.synthesizeKey("VK_F1", {}, toolbox.win);
  await onReady;
}

async function clickSettingsMenu(toolbox) {
  const onPopupShown = () => {
    toolbox.doc.removeEventListener("popuphidden", onPopupShown);
    const menuItem = toolbox.doc.getElementById("toolbox-meatball-menu-settings");
    EventUtils.synthesizeMouseAtCenter(menuItem, {}, menuItem.ownerGlobal);
  };
  toolbox.doc.addEventListener("popupshown", onPopupShown);

  const button = toolbox.doc.getElementById("toolbox-meatball-menu-button");
  EventUtils.synthesizeMouseAtCenter(button, {}, button.ownerGlobal);

  await toolbox.once("select");
}
