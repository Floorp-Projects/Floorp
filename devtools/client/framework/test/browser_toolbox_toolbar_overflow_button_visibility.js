/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for the toolbox tabs rearrangement when the visibility of toolbox buttons were changed.

const { Toolbox } = require("devtools/client/framework/toolbox");

add_task(async function() {
  const tab = await addTab("about:blank");
  const toolbox = await openToolboxForTab(tab, "options", Toolbox.HostType.BOTTOM);
  const toolboxButtonPreferences =
    toolbox.toolbarButtons.map(button => button.visibilityswitch);

  const win = getWindow(toolbox);
  const { outerWidth: originalWindowWidth, outerHeight: originalWindowHeight } = win;
  registerCleanupFunction(() => {
    for (const preference of toolboxButtonPreferences) {
      Services.prefs.clearUserPref(preference);
    }

    win.resizeTo(originalWindowWidth, originalWindowHeight);
  });

  const optionsTool = toolbox.getCurrentPanel();
  const checkButtons =
    optionsTool.panelWin.document
               .querySelectorAll("#enabled-toolbox-buttons-box input[type=checkbox]");

  info("Test the count of shown devtools tab after making all buttons to be visible");
  await resizeWindow(toolbox, 800);
  // Once, make all toolbox button to be invisible.
  setToolboxButtonsVisibility(checkButtons, false);
  // Get count of shown devtools tab elements.
  const initialTabCount = toolbox.doc.querySelectorAll(".devtools-tab").length;
  // Make all toolbox button to be visible.
  setToolboxButtonsVisibility(checkButtons, true);
  ok(toolbox.doc.querySelectorAll(".devtools-tab").length < initialTabCount,
     "Count of shown devtools tab should decreased");

  info("Test the count of shown devtools tab after making all buttons to be invisible");
  setToolboxButtonsVisibility(checkButtons, false);
  is(toolbox.doc.querySelectorAll(".devtools-tab").length, initialTabCount,
     "Count of shown devtools tab should be same to 1st count");
});

function setToolboxButtonsVisibility(checkButtons, doVisible) {
  for (const checkButton of checkButtons) {
    if (checkButton.checked === doVisible) {
      continue;
    }

    checkButton.click();
  }
}
