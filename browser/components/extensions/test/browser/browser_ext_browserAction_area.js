/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

var browserAreas = {
  navbar: CustomizableUI.AREA_NAVBAR,
  menupanel: getCustomizableUIPanelID(),
  tabstrip: CustomizableUI.AREA_TABSTRIP,
  personaltoolbar: CustomizableUI.AREA_BOOKMARKS,
};

async function testInArea(area) {
  let manifest = {
    browser_action: {
      browser_style: true,
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
  let fallbackDefaultArea = CustomizableUI.AREA_ADDONS;
  is(
    placement && placement.area,
    browserAreas[area] || fallbackDefaultArea,
    `widget located in correct area`
  );
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

add_task(async function testPolicyOverridesBrowserActionToNavbar() {
  const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
    "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
  );
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      ExtensionSettings: {
        "policyBrowserActionAreaNavBarTest@mozilla.com": {
          default_area: "navbar",
        },
      },
    },
  });
  let manifest = {
    browser_action: {},
    browser_specific_settings: {
      gecko: {
        id: "policyBrowserActionAreaNavBarTest@mozilla.com",
      },
    },
  };
  let extension = ExtensionTestUtils.loadExtension({
    manifest,
  });
  await extension.startup();
  let widget = getBrowserActionWidget(extension);
  let placement = CustomizableUI.getPlacementOfWidget(widget.id);
  is(
    placement && placement.area,
    CustomizableUI.AREA_NAVBAR,
    `widget located in nav bar`
  );
  await extension.unload();
});

add_task(async function testPolicyOverridesBrowserActionToMenuPanel() {
  const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
    "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
  );
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      ExtensionSettings: {
        "policyBrowserActionAreaMenuPanelTest@mozilla.com": {
          default_area: "menupanel",
        },
      },
    },
  });
  let manifest = {
    browser_action: {
      default_area: "navbar",
    },
    browser_specific_settings: {
      gecko: {
        id: "policyBrowserActionAreaMenuPanelTest@mozilla.com",
      },
    },
  };
  let extension = ExtensionTestUtils.loadExtension({
    manifest,
  });
  await extension.startup();
  let widget = getBrowserActionWidget(extension);
  let placement = CustomizableUI.getPlacementOfWidget(widget.id);
  is(
    placement && placement.area,
    getCustomizableUIPanelID(),
    `widget located in extensions menu`
  );
  await extension.unload();
});
