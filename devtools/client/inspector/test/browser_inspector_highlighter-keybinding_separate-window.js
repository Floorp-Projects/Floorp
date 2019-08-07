/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the keybindings for element picker with separate window.

const { Toolbox } = require("devtools/client/framework/toolbox");

const IS_OSX = Services.appinfo.OS === "Darwin";
const TEST_URL = "data:text/html;charset=utf8,<div></div>";

const TEST_MODIFIERS = [
  {
    isOSX: true,
    description: "OSX and altKey+metaKey",
    modifier: { altKey: true, metaKey: true },
  },
  {
    isOSX: true,
    description: "OSX and metaKey+shiftKey",
    modifier: { metaKey: true, shiftKey: true },
  },
  {
    description: "ctrlKey+shiftKey",
    modifier: { ctrlKey: true, shiftKey: true },
  },
];

add_task(async () => {
  info("In order to open the inspector in separate window");
  await pushPref("devtools.toolbox.host", "window");

  info("Open an inspected tab");
  await addTab(TEST_URL);

  for (const { description, modifier, isOSX } of TEST_MODIFIERS) {
    if (!!isOSX !== IS_OSX) {
      continue;
    }

    info(`Start the test for ${description}`);

    info("Open the toolbox and the inspecor");
    const onToolboxReady = gDevTools.once("toolbox-ready");
    EventUtils.synthesizeKey("c", modifier, window);

    info("Check the state of the inspector");
    const toolbox = await onToolboxReady;
    is(
      toolbox.hostType,
      Toolbox.HostType.WINDOW,
      "The toolbox opens in a separate window"
    );
    is(toolbox.currentToolId, "inspector", "The inspector selects");
    await assertStatuses(toolbox, true, true);

    info("Toggle the picker mode by the shortcut key on the toolbox");
    EventUtils.synthesizeKey("c", modifier, toolbox.win);
    await assertStatuses(toolbox, false, true);

    info("Focus on main window");
    window.focus();
    await waitForFocusChanged(document, true);
    ok(true, "The main window has focus");

    info("Toggle the picker mode by the shortcut key on the main window");
    EventUtils.synthesizeKey("c", modifier, window);
    await assertStatuses(toolbox, true, false);

    info("Toggle again by the shortcut key on the main window");
    EventUtils.synthesizeKey("c", modifier, window);
    await assertStatuses(toolbox, false, false);

    info("Close the toolbox");
    await toolbox.closeToolbox();
  }
});

async function assertStatuses(toolbox, isPickerMode, isToolboxHavingFocus) {
  info("Check the state of the picker mode");
  await waitUntil(() => toolbox.pickerButton.isChecked === isPickerMode);
  is(
    toolbox.pickerButton.isChecked,
    isPickerMode,
    "The picker mode is correct"
  );

  info("Check whether the toolbox has the focus");
  await waitForFocusChanged(toolbox.doc, isToolboxHavingFocus);
  ok(true, "The focus state of the toolbox is correct");

  await waitForFocusChanged(document, !isToolboxHavingFocus);
  ok(true, "The focus state of the main window is correct");
}

async function waitForFocusChanged(doc, expected) {
  return waitUntil(() => doc.hasFocus() === expected);
}
