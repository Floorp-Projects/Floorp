/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the Filter Editor Tooltip reverting changes on ESC

const TEST_URL = TEST_URL_ROOT + "doc_filter.html";

add_task(function*() {
  yield addTab(TEST_URL);

  let {toolbox, inspector, view} = yield openRuleView();

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

  info("Pressing ESCAPE to close the tooltip");
  EventUtils.sendKey("ESCAPE", widget.doc.defaultView);

  yield waitForSuccess(() => {
    const computed = content.getComputedStyle(content.document.body);
    return computed.filter === "blur(2px) contrast(2)";
  }, "Waiting for the change to be reverted on the element");
});
