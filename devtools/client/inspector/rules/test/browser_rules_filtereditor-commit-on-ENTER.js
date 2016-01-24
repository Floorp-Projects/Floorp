/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the Filter Editor Tooltip committing changes on ENTER

const TEST_URL = URL_ROOT + "doc_filter.html";

add_task(function*() {
  yield addTab(TEST_URL);
  let {view} = yield openRuleView();

  info("Getting the filter swatch element");
  let swatch = getRuleViewProperty(view, "body", "filter").valueSpan
    .querySelector(".ruleview-filterswatch");

  let filterTooltip = view.tooltips.filterEditor;
  // Clicking on a cssfilter swatch sets the current filter value in the tooltip
  // which, in turn, makes the FilterWidget emit an "updated" event that causes
  // the rule-view to refresh. So we must wait for the ruleview-changed event.
  let onRuleViewChanged = view.once("ruleview-changed");
  swatch.click();
  yield onRuleViewChanged;

  let widget = yield filterTooltip.widget;

  onRuleViewChanged = view.once("ruleview-changed");
  widget.setCssValue("blur(2px)");
  yield waitForComputedStyleProperty("body", null, "filter", "blur(2px)");
  yield onRuleViewChanged;

  ok(true, "Changes previewed on the element");

  onRuleViewChanged = view.once("ruleview-changed");
  info("Pressing RETURN to commit changes");
  EventUtils.sendKey("RETURN", widget.styleWindow);
  yield onRuleViewChanged;

  const computed = content.getComputedStyle(content.document.body);
  is(computed.filter, "blur(2px)",
     "The elemenet's filter was kept after RETURN");
});
