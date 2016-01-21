/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the that Filter Editor Tooltip opens by clicking on filter swatches

const TEST_URL = URL_ROOT + "doc_filter.html";

add_task(function*() {
  yield addTab(TEST_URL);

  let {view} = yield openRuleView();

  info("Getting the filter swatch element");
  let swatch = getRuleViewProperty(view, "body", "filter").valueSpan
    .querySelector(".ruleview-filterswatch");

  let filterTooltip = view.tooltips.filterEditor;
  let onShow = filterTooltip.tooltip.once("shown");
  let onRuleViewChanged = view.once("ruleview-changed");
  swatch.click();
  yield onShow;
  // Clicking on swatch runs a preview of the current value
  // and updates the rule-view
  yield onRuleViewChanged;

  ok(true, "The shown event was emitted after clicking on swatch");
  ok(!inplaceEditor(swatch.parentNode),
  "The inplace editor wasn't shown as a result of the filter swatch click");

  yield filterTooltip.widget;
  filterTooltip.hide();
});
