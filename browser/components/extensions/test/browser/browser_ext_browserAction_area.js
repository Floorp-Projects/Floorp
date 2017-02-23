/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

var browserAreas = {
  "navbar": CustomizableUI.AREA_NAVBAR,
  "menupanel": CustomizableUI.AREA_PANEL,
  "tabstrip": CustomizableUI.AREA_TABSTRIP,
  "personaltoolbar": CustomizableUI.AREA_BOOKMARKS,
};

function* testInArea(area) {
  let manifest = {
    "browser_action": {
      "browser_style": true,
    },
  };
  if (area) {
    manifest.browser_action.default_area = area;
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest,
  });
  yield extension.startup();
  let widget = getBrowserActionWidget(extension);
  let placement = CustomizableUI.getPlacementOfWidget(widget.id);
  is(placement && placement.area, browserAreas[area || "navbar"], `widget located in correct area`);
  yield extension.unload();
}

add_task(function* testBrowserActionDefaultArea() {
  yield testInArea();
});

add_task(function* testBrowserActionInToolbar() {
  yield testInArea("navbar");
});

add_task(function* testBrowserActionInMenuPanel() {
  yield testInArea("menupanel");
});

add_task(function* testBrowserActionInTabStrip() {
  yield testInArea("tabstrip");
});

add_task(function* testBrowserActionInPersonalToolbar() {
  yield testInArea("personaltoolbar");
});
