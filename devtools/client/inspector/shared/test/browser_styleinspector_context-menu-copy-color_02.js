/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test "Copy color" item of the context menu #2: Test that correct color is
// copied if the color changes.

const TEST_URI = `
  <style type="text/css">
    div {
      color: #123ABC;
    }
  </style>
  <div>Testing the color picker tooltip!</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { inspector, view } = await openRuleView();

  await testCopyToClipboard(inspector, view);
  await testManualEdit(inspector, view);
  await testColorPickerEdit(inspector, view);
});

async function testCopyToClipboard(inspector, view) {
  info("Testing that color is copied to clipboard");

  await selectNode("div", inspector);

  const element = getRuleViewProperty(
    view,
    "div",
    "color"
  ).valueSpan.querySelector(".ruleview-colorswatch");

  const allMenuItems = openStyleContextMenuAndGetAllItems(view, element);
  const menuitemCopyColor = allMenuItems.find(
    item =>
      item.label ===
      STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyColor")
  );

  ok(menuitemCopyColor.visible, "Copy color is visible");

  await waitForClipboardPromise(() => menuitemCopyColor.click(), "#123ABC");

  EventUtils.synthesizeKey("KEY_Escape");
}

async function testManualEdit(inspector, view) {
  info("Testing manually edited colors");
  await selectNode("div", inspector);

  const { valueSpan } = getRuleViewProperty(view, "div", "color");

  const newColor = "#C9184E";
  const editor = await focusEditableField(view, valueSpan);

  info("Typing new value");
  const input = editor.input;
  const onBlur = once(input, "blur");
  EventUtils.sendString(newColor + ";", view.styleWindow);
  await onBlur;
  await wait(1);

  const colorValueElement = getRuleViewProperty(view, "div", "color").valueSpan
    .firstChild;
  is(colorValueElement.dataset.color, newColor, "data-color was updated");

  const contextMenu = view.contextMenu;
  contextMenu.currentTarget = colorValueElement;
  contextMenu._isColorPopup();

  is(contextMenu._colorToCopy, newColor, "_colorToCopy has the new value");
}

async function testColorPickerEdit(inspector, view) {
  info("Testing colors edited via color picker");
  await selectNode("div", inspector);

  const swatchElement = getRuleViewProperty(
    view,
    "div",
    "color"
  ).valueSpan.querySelector(".ruleview-colorswatch");

  info("Opening the color picker");
  const picker = view.tooltips.getTooltip("colorPicker");
  const onColorPickerReady = picker.once("ready");
  swatchElement.click();
  await onColorPickerReady;

  const newColor = "#53B759";
  const { colorUtils } = require("devtools/shared/css/color");

  const { r, g, b, a } = new colorUtils.CssColor(newColor).getRGBATuple();
  await simulateColorPickerChange(view, picker, [r, g, b, a]);

  is(
    swatchElement.parentNode.dataset.color,
    newColor,
    "data-color was updated"
  );

  const contextMenu = view.contextMenu;
  contextMenu.currentTarget = swatchElement;
  contextMenu._isColorPopup();

  is(contextMenu._colorToCopy, newColor, "_colorToCopy has the new value");
}
