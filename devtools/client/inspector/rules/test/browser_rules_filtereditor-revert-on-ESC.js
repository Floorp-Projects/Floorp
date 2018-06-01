/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that changes made to the Filter Editor Tooltip are reverted when
// ESC is pressed

const TEST_URL = URL_ROOT + "doc_filter.html";

add_task(async function() {
  await addTab(TEST_URL);
  const {view} = await openRuleView();
  await testPressingEscapeRevertsChanges(view);
});

async function testPressingEscapeRevertsChanges(view) {
  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const propEditor = ruleEditor.rule.textProps[0].editor;
  const swatch = propEditor.valueSpan.querySelector(".ruleview-filterswatch");

  await clickOnFilterSwatch(swatch, view);
  await setValueInFilterWidget("blur(2px)", view);

  await waitForComputedStyleProperty("body", null, "filter", "blur(2px)");
  is(propEditor.valueSpan.textContent, "blur(2px)",
    "Got expected property value.");

  await pressEscapeToCloseTooltip(view);

  await waitForComputedStyleProperty("body", null, "filter",
    "blur(2px) contrast(2)");
  is(propEditor.valueSpan.textContent, "blur(2px) contrast(2)",
    "Got expected property value.");
}

async function clickOnFilterSwatch(swatch, view) {
  info("Clicking on a css filter swatch to open the tooltip");

  // Clicking on a cssfilter swatch sets the current filter value in the tooltip
  // which, in turn, makes the FilterWidget emit an "updated" event that causes
  // the rule-view to refresh. So we must wait for the ruleview-changed event.
  const onRuleViewChanged = view.once("ruleview-changed");
  swatch.click();
  await onRuleViewChanged;
}

async function setValueInFilterWidget(value, view) {
  info("Setting the CSS filter value in the tooltip");

  const filterTooltip = view.tooltips.getTooltip("filterEditor");
  const onRuleViewChanged = view.once("ruleview-changed");
  filterTooltip.widget.setCssValue(value);
  await onRuleViewChanged;
}

async function pressEscapeToCloseTooltip(view) {
  info("Pressing ESCAPE to close the tooltip");

  const filterTooltip = view.tooltips.getTooltip("filterEditor");
  const onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.sendKey("ESCAPE", filterTooltip.widget.styleWindow);
  await onRuleViewChanged;
}
