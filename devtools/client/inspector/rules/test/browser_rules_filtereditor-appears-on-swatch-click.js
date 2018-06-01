/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the that Filter Editor Tooltip opens by clicking on filter swatches

const TEST_URL = URL_ROOT + "doc_filter.html";

add_task(async function() {
  await addTab(TEST_URL);

  const {view} = await openRuleView();

  info("Getting the filter swatch element");
  const swatch = getRuleViewProperty(view, "body", "filter").valueSpan
    .querySelector(".ruleview-filterswatch");

  const filterTooltip = view.tooltips.getTooltip("filterEditor");
  // Clicking on a cssfilter swatch sets the current filter value in the tooltip
  // which, in turn, makes the FilterWidget emit an "updated" event that causes
  // the rule-view to refresh. So we must wait for the ruleview-changed event.
  const onRuleViewChanged = view.once("ruleview-changed");
  swatch.click();
  await onRuleViewChanged;

  ok(true, "The shown event was emitted after clicking on swatch");
  ok(!inplaceEditor(swatch.parentNode),
  "The inplace editor wasn't shown as a result of the filter swatch click");

  await hideTooltipAndWaitForRuleViewChanged(filterTooltip, view);

  await waitForTick();
});
