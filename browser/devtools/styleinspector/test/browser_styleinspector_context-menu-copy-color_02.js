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

  let win = view.doc.defaultView;
  let element = getRuleViewProperty(view, "div", "color").valueSpan
    .querySelector(".ruleview-colorswatch");

  let popup = once(view._contextmenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(element, {button: 2, type: "contextmenu"}, win);
  yield popup;

  ok(!view.menuitemCopyColor.hidden, "Copy color is visible");

  yield waitForClipboard(() => view.menuitemCopyColor.click(), TEST_COLOR);
  view._contextmenu.hidePopup();
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
  EventUtils.sendString(newColor + ";", view.doc.defaultView);
  yield onBlur;
  yield wait(1);

  let colorValue = getRuleViewProperty(view, "div", "color").valueSpan.firstChild;
  is(colorValue.dataset.color, newColor, "data-color was updated");

  view.doc.popupNode = colorValue;
  view._isColorPopup();

  is(view._colorToCopy, newColor, "_colorToCopy has the new value");
}

function* testColorPickerEdit(inspector, view) {
  info("Testing colors edited via color picker");
  yield selectNode("div", inspector);

  let swatch = getRuleViewProperty(view, "div", "color").valueSpan
    .querySelector(".ruleview-colorswatch");

  info("Opening the color picker");
  let picker = view.tooltips.colorPicker;
  let onShown = picker.tooltip.once("shown");
  swatch.click();
  yield onShown;

  let rgbaColor = [83, 183, 89, 1];
  let rgbaColorText = "rgba(83, 183, 89, 1)";
  yield simulateColorPickerChange(view, picker, rgbaColor);

  is(swatch.parentNode.dataset.color, rgbaColorText, "data-color was updated");
  view.doc.popupNode = swatch;
  view._isColorPopup();

  is(view._colorToCopy, rgbaColorText, "_colorToCopy has the new value");
}
