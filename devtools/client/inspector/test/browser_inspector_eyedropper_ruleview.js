/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CSS_URI = URL_ROOT + "style_inspector_eyedropper_ruleview.css";

const TEST_URI = `
  <link href="${CSS_URI}" rel="stylesheet" type="text/css"/>
`;

// Test that opening the eyedropper before opening devtools doesn't break links
// in the ruleview.
add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const onPickerCommandHandled = new Promise(r => {
    const listener = subject => {
      Services.obs.removeObserver(listener, "color-picker-command-handled");
      r(subject.wrappedJSObject);
    };
    Services.obs.addObserver(listener, "color-picker-command-handled");
  });

  info("Trigger the eyedropper command");
  const menu = document.getElementById("menu_eyedropper");
  menu.doCommand();

  info("Wait for the color-picker-command-handled observable");
  const targetFront = await onPickerCommandHandled;

  info("Wait for the eye dropper to be visible");
  const highlighterTestFront = await targetFront.getFront("highlighterTest");
  await asyncWaitUntil(() => highlighterTestFront.isEyeDropperVisible());

  info("Cancel the eye dropper and wait for the target to be destroyed");
  EventUtils.synthesizeKey("KEY_Escape");
  await waitFor(() => targetFront.isDestroyed());

  const { inspector, view } = await openRuleView();

  await selectNode("body", inspector);

  const linkText = getRuleViewLinkTextByIndex(view, 1);
  is(
    linkText,
    "style_inspector_eyedropper_ruleview.css:1",
    "link text at index 1 has the correct link."
  );
});
