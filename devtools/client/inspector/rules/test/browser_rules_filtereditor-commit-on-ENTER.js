/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the Filter Editor Tooltip committing changes on ENTER

const TEST_URL = URL_ROOT + "doc_filter.html";

add_task(function* () {
  yield addTab(TEST_URL);
  let {view} = yield openRuleView();

  info("Get the filter swatch element");
  let swatch = getRuleViewProperty(view, "body", "filter").valueSpan
    .querySelector(".ruleview-filterswatch");

  info("Click on the filter swatch element");
  // Clicking on a cssfilter swatch sets the current filter value in the tooltip
  // which, in turn, makes the FilterWidget emit an "updated" event that causes
  // the rule-view to refresh. So we must wait for the ruleview-changed event.
  let onRuleViewChanged = view.once("ruleview-changed");
  swatch.click();
  yield onRuleViewChanged;

  info("Get the cssfilter widget instance");
  let filterTooltip = view.tooltips.filterEditor;
  let widget = filterTooltip.widget;

  info("Set a new value in the cssfilter widget");
  onRuleViewChanged = view.once("ruleview-changed");
  widget.setCssValue("blur(2px)");
  yield waitForComputedStyleProperty("body", null, "filter", "blur(2px)");
  yield onRuleViewChanged;
  ok(true, "Changes previewed on the element");

  info("Press RETURN to commit changes");
  // Pressing return in the cssfilter tooltip triggeres 2 ruleview-changed
  onRuleViewChanged = waitForNEvents(view, "ruleview-changed", 2);
  EventUtils.sendKey("RETURN", widget.styleWindow);
  yield onRuleViewChanged;

  is((yield getComputedStyleProperty("body", null, "filter")), "blur(2px)",
     "The elemenet's filter was kept after RETURN");
});
