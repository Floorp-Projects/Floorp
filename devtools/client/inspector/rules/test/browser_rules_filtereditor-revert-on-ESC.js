/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that changes made to the Filter Editor Tooltip are reverted when
// ESC is pressed

const TEST_URL = URL_ROOT + "doc_filter.html";

add_task(function* () {
  yield addTab(TEST_URL);
  let {view} = yield openRuleView();
  yield testPressingEscapeRevertsChanges(view);
});

function* testPressingEscapeRevertsChanges(view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;
  let swatch = propEditor.valueSpan.querySelector(".ruleview-filterswatch");

  yield clickOnFilterSwatch(swatch, view);
  yield setValueInFilterWidget("blur(2px)", view);

  yield waitForComputedStyleProperty("body", null, "filter", "blur(2px)");
  is(propEditor.valueSpan.textContent, "blur(2px)",
    "Got expected property value.");

  yield pressEscapeToCloseTooltip(view);

  yield waitForComputedStyleProperty("body", null, "filter",
    "blur(2px) contrast(2)");
  is(propEditor.valueSpan.textContent, "blur(2px) contrast(2)",
    "Got expected property value.");
}

function* clickOnFilterSwatch(swatch, view) {
  info("Clicking on a css filter swatch to open the tooltip");

  // Clicking on a cssfilter swatch sets the current filter value in the tooltip
  // which, in turn, makes the FilterWidget emit an "updated" event that causes
  // the rule-view to refresh. So we must wait for the ruleview-changed event.
  let onRuleViewChanged = view.once("ruleview-changed");
  swatch.click();
  yield onRuleViewChanged;
}

function* setValueInFilterWidget(value, view) {
  info("Setting the CSS filter value in the tooltip");

  let filterTooltip = view.tooltips.getTooltip("filterEditor");
  let onRuleViewChanged = view.once("ruleview-changed");
  filterTooltip.widget.setCssValue(value);
  yield onRuleViewChanged;
}

function* pressEscapeToCloseTooltip(view) {
  info("Pressing ESCAPE to close the tooltip");

  let filterTooltip = view.tooltips.getTooltip("filterEditor");
  let onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.sendKey("ESCAPE", filterTooltip.widget.styleWindow);
  yield onRuleViewChanged;
}
