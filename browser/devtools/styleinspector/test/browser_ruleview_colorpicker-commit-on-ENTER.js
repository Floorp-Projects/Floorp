/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a color change in the color picker is committed when ENTER is pressed

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    border: 2em solid rgba(120, 120, 120, .5);',
  '  }',
  '</style>',
  'Testing the color picker tooltip!'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,rule view color picker tooltip test");
  content.document.body.innerHTML = PAGE_CONTENT;
  let {toolbox, inspector, view} = yield openRuleView();

  let swatch = getRuleViewProperty(view, "body" , "border").valueSpan
    .querySelector(".ruleview-colorswatch");
  yield testPressingEnterCommitsChanges(swatch, view);
});

function* testPressingEnterCommitsChanges(swatch, ruleView) {
  let cPicker = ruleView.tooltips.colorPicker;

  let onShown = cPicker.tooltip.once("shown");
  swatch.click();
  yield onShown;

  yield simulateColorPickerChange(ruleView, cPicker, [0, 255, 0, .5], {
    element: content.document.body,
    name: "borderLeftColor",
    value: "rgba(0, 255, 0, 0.5)"
  });

  is(swatch.style.backgroundColor, "rgba(0, 255, 0, 0.5)",
    "The color swatch's background was updated");
  is(getRuleViewProperty(ruleView, "body", "border").valueSpan.textContent,
    "2em solid rgba(0, 255, 0, 0.5)",
    "The text of the border css property was updated");;

  let spectrum = yield cPicker.spectrum;
  let onHidden = cPicker.tooltip.once("hidden");
  EventUtils.sendKey("RETURN", spectrum.element.ownerDocument.defaultView);
  yield onHidden;

  is(content.getComputedStyle(content.document.body).borderLeftColor,
    "rgba(0, 255, 0, 0.5)", "The element's border was kept after RETURN");
  is(swatch.style.backgroundColor, "rgba(0, 255, 0, 0.5)",
    "The color swatch's background was kept after RETURN");
  is(getRuleViewProperty(ruleView, "body", "border").valueSpan.textContent,
    "2em solid rgba(0, 255, 0, 0.5)",
    "The text of the border css property was kept after RETURN");
}
