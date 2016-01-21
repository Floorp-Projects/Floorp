/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that changes made to the Filter Editor Tooltip are reverted when
// ESC is pressed

const TEST_URL = URL_ROOT + "doc_filter.html";

add_task(function*() {
  yield addTab(TEST_URL);
  let {view} = yield openRuleView();
  yield testPressingEscapeRevertsChanges(view);
  yield testPressingEscapeRevertsChangesAndDisables(view);
});

function* testPressingEscapeRevertsChanges(view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;
  let swatch = propEditor.valueSpan.querySelector(".ruleview-filterswatch");
  let filterTooltip = view.tooltips.filterEditor;

  let onShow = filterTooltip.tooltip.once("shown");
  swatch.click();
  yield onShow;

  let widget = yield filterTooltip.widget;
  widget.setCssValue("blur(2px)");
  yield ruleEditor.rule._applyingModifications;

  yield waitForComputedStyleProperty("body", null, "filter", "blur(2px)");
  is(propEditor.valueSpan.textContent, "blur(2px)",
    "Got expected property value.");

  info("Pressing ESCAPE to close the tooltip");
  EventUtils.sendKey("ESCAPE", widget.styleWindow);
  yield ruleEditor.rule._applyingModifications;

  yield waitForComputedStyleProperty("body", null, "filter",
    "blur(2px) contrast(2)");
  is(propEditor.valueSpan.textContent, "blur(2px) contrast(2)",
    "Got expected property value.");
}

function* testPressingEscapeRevertsChangesAndDisables(view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;
  let swatch = propEditor.valueSpan.querySelector(".ruleview-filterswatch");
  let filterTooltip = view.tooltips.filterEditor;

  info("Disabling filter property");
  propEditor.enable.click();
  yield ruleEditor.rule._applyingModifications;

  ok(propEditor.element.classList.contains("ruleview-overridden"),
    "property is overridden.");
  is(propEditor.enable.style.visibility, "visible",
    "property enable checkbox is visible.");
  ok(!propEditor.enable.getAttribute("checked"),
    "property enable checkbox is not checked.");
  ok(!propEditor.prop.enabled,
    "filter property is disabled.");
  let newValue = yield getRulePropertyValue("filter");
  is(newValue, "", "filter should have been unset.");

  let onShow = filterTooltip.tooltip.once("shown");
  swatch.click();
  yield onShow;

  ok(!propEditor.element.classList.contains("ruleview-overridden"),
    "property overridden is not displayed.");
  is(propEditor.enable.style.visibility, "hidden",
    "property enable checkbox is hidden.");

  let widget = yield filterTooltip.widget;
  widget.setCssValue("blur(2px)");
  yield ruleEditor.rule._applyingModifications;

  info("Pressing ESCAPE to close the tooltip");
  EventUtils.sendKey("ESCAPE", widget.styleWindow);
  yield ruleEditor.rule._applyingModifications;

  ok(propEditor.element.classList.contains("ruleview-overridden"),
    "property is overridden.");
  is(propEditor.enable.style.visibility, "visible",
    "property enable checkbox is visible.");
  ok(!propEditor.enable.getAttribute("checked"),
    "property enable checkbox is not checked.");
  ok(!propEditor.prop.enabled, "filter property is disabled.");
  newValue = yield getRulePropertyValue("filter");
  is(newValue, "", "filter should have been unset.");
  is(propEditor.valueSpan.textContent, "blur(2px) contrast(2)",
    "Got expected property value.");
}

function* getRulePropertyValue(name) {
  let propValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: name
  });
  return propValue;
}
