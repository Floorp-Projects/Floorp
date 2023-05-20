/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the that Filter Editor Tooltip opens by clicking on filter swatches

const TEST_URL = URL_ROOT + "doc_filter.html";

add_task(async function () {
  await addTab(TEST_URL);

  const { toolbox, view } = await openRuleView();

  info("Getting the filter swatch element");
  const property = await getRuleViewProperty(view, "body", "filter", {
    wait: true,
  });

  const swatch = property.valueSpan.querySelector(".ruleview-filterswatch");

  const filterTooltip = view.tooltips.getTooltip("filterEditor");
  // Clicking on a cssfilter swatch sets the current filter value in the tooltip
  // which, in turn, makes the FilterWidget emit an "updated" event that causes
  // the rule-view to refresh. So we must wait for the ruleview-changed event.
  const onRuleViewChanged = view.once("ruleview-changed");
  swatch.click();
  await onRuleViewChanged;

  ok(true, "The shown event was emitted after clicking on swatch");
  ok(
    !inplaceEditor(swatch.parentNode),
    "The inplace editor wasn't shown as a result of the filter swatch click"
  );

  info("Get the cssfilter widget instance");
  const widget = filterTooltip.widget;
  const select = widget.el.querySelector("select");

  // Next we will check that interacting with the select does not close the
  // filter tooltip.
  info("Show the filter select");
  const onSelectPopupShown = BrowserTestUtils.waitForSelectPopupShown(window);
  EventUtils.synthesizeMouseAtCenter(select, {}, toolbox.win);
  const selectPopup = await onSelectPopupShown;
  ok(
    filterTooltip.tooltip.isVisible(),
    "The tooltip was not hidden when opening the select"
  );

  info("Hide the filter select");
  const onSelectPopupHidden = once(selectPopup, "popuphidden");
  const blurMenuItem = selectPopup.querySelector("menuitem[label='blur']");
  EventUtils.synthesizeMouseAtCenter(blurMenuItem, {}, window);
  await onSelectPopupHidden;
  await waitFor(() => select.value === "blur");
  is(
    select.value,
    "blur",
    "The filter select was updated with the correct value"
  );
  ok(
    filterTooltip.tooltip.isVisible(),
    "The tooltip was not hidden when using the select"
  );

  await hideTooltipAndWaitForRuleViewChanged(filterTooltip, view);
  await waitForTick();
});
