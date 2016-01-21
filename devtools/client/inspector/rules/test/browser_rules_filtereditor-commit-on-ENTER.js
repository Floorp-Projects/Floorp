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
  let onShow = filterTooltip.tooltip.once("shown");
  swatch.click();
  yield onShow;

  let widget = yield filterTooltip.widget;

  widget.setCssValue("blur(2px)");
  yield waitForComputedStyleProperty("body", null, "filter", "blur(2px)");

  ok(true, "Changes previewed on the element");

  info("Pressing RETURN to commit changes");
  EventUtils.sendKey("RETURN", widget.styleWindow);

  const computed = content.getComputedStyle(content.document.body);
  is(computed.filter, "blur(2px)",
     "The elemenet's filter was kept after RETURN");
});
