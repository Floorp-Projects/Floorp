/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

var browserAreas = {
  "navbar": CustomizableUI.AREA_NAVBAR,
  "menupanel": getCustomizableUIPanelID(),
  "tabstrip": CustomizableUI.AREA_TABSTRIP,
  "personaltoolbar": CustomizableUI.AREA_BOOKMARKS,
};

async function testInArea(area) {
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
  await extension.startup();
  let widget = getBrowserActionWidget(extension);
  let placement = CustomizableUI.getPlacementOfWidget(widget.id);
  is(placement && placement.area, browserAreas[area || "navbar"], `widget located in correct area`);
  await extension.unload();
}

add_task(async function testBrowserActionDefaultArea() {
  await testInArea();
});

add_task(async function testBrowserActionInToolbar() {
  await testInArea("navbar");
});

add_task(async function testBrowserActionInMenuPanel() {
  await testInArea("menupanel");
});

add_task(async function testBrowserActionInTabStrip() {
  await testInArea("tabstrip");
});

add_task(async function testBrowserActionInPersonalToolbar() {
  await testInArea("personaltoolbar");
});
