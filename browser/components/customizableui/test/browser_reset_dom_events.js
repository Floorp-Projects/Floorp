"use strict";

const widgetId = "import-button";
const listener = {
  _beforeCount: 0,
  _afterCount: 0,
  onWidgetBeforeDOMChange(node) {
    if (node.id == widgetId) {
      this._beforeCount++;
    }
  },
  onWidgetAfterDOMChange(node) {
    if (node.id == widgetId) {
      this._afterCount++;
    }
  },
};

add_task(async function test_reset_dom_events() {
  await startCustomizing();

  CustomizableUI.addWidgetToArea(widgetId, CustomizableUI.AREA_BOOKMARKS);
  CustomizableUI.addListener(listener);

  info("Resetting");
  await gCustomizeMode.reset();

  is(listener._beforeCount, 1, "Should've been notified of the mutation");
  is(listener._afterCount, 1, "Should've been notified of the mutation");

  CustomizableUI.removeListener(listener);

  await endCustomizing();
});
