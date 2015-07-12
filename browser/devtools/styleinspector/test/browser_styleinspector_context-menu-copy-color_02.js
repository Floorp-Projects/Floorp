/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test "Copy color" item of the context menu #2: Test that correct color is
// copied if the color changes.

const TEST_COLOR = "#123ABC";

add_task(function* () {
  const PAGE_CONTENT = [
    '<style type="text/css">',
    '  div {',
    '    color: ' + TEST_COLOR + ';',
    '  }',
    '</style>',
    '<div>Testing the color picker tooltip!</div>'
  ].join("\n");

  yield addTab("data:text/html;charset=utf8,Test context menu Copy color");
  content.document.body.innerHTML = PAGE_CONTENT;

  let {inspector, view} = yield openRuleView();
  yield testCopyToClipboard(inspector, view);
  yield testManualEdit(inspector, view);
  yield testColorPickerEdit(inspector, view);
});

function* testCopyToClipboard(inspector, view) {
  info("Testing that color is copied to clipboard");

  yield selectNode("div", inspector);

  let win = view.styleWindow;
  let element = getRuleViewProperty(view, "div", "color").valueSpan
    .querySelector(".ruleview-colorswatch");

  let popup = once(view._contextmenu._menupopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(element, {button: 2, type: "contextmenu"}, win);
  yield popup;

  ok(!view._contextmenu.menuitemCopyColor.hidden, "Copy color is visible");

  yield waitForClipboard(() => view._contextmenu.menuitemCopyColor.click(), TEST_COLOR);
  view._contextmenu._menupopup.hidePopup();
}

function* testManualEdit(inspector, view) {
  info("Testing manually edited colors");
  yield selectNode("div", inspector);

  let {valueSpan} = getRuleViewProperty(view, "div", "color");

  let newColor = "#C9184E"
  let editor = yield focusEditableField(view, valueSpan);

  info("Typing new value");
  let input = editor.input;
  let onBlur = once(input, "blur");
  EventUtils.sendString(newColor + ";", view.styleWindow);
  yield onBlur;
  yield wait(1);

  let colorValueElement = getRuleViewProperty(view, "div", "color").valueSpan.firstChild;
  is(colorValueElement.dataset.color, newColor, "data-color was updated");

  view.styleDocument.popupNode = colorValueElement;

  let contextMenu = view._contextmenu;
  contextMenu._isColorPopup();
  is(contextMenu._colorToCopy, newColor, "_colorToCopy has the new value");
}

function* testColorPickerEdit(inspector, view) {
  info("Testing colors edited via color picker");
  yield selectNode("div", inspector);

  let swatchElement = getRuleViewProperty(view, "div", "color").valueSpan
    .querySelector(".ruleview-colorswatch");

  info("Opening the color picker");
  let picker = view.tooltips.colorPicker;
  let onShown = picker.tooltip.once("shown");
  swatchElement.click();
  yield onShown;

  let rgbaColor = [83, 183, 89, 1];
  let rgbaColorText = "rgba(83, 183, 89, 1)";
  yield simulateColorPickerChange(view, picker, rgbaColor);

  is(swatchElement.parentNode.dataset.color, rgbaColorText, "data-color was updated");
  view.styleDocument.popupNode = swatchElement;

  let contextMenu = view._contextmenu;
  contextMenu._isColorPopup();
  is(contextMenu._colorToCopy, rgbaColorText, "_colorToCopy has the new value");
}
