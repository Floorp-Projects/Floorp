/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gTests = [
  {
    desc: "Make sure we can read customizableui-areatypes from XUL widgets",
    run: function() {
      // Create some XUL toolbar items and put them in the palette.
      let items = {
        "test-item1": {
          properties: {
            "customizableui-areatypes": [CustomizableUI.AREATYPE_MENU_PANEL,
                                         CustomizableUI.AREATYPE_TOOLBAR].join(",")
          },
          expectedSet: new Set([CustomizableUI.AREATYPE_MENU_PANEL,
                                CustomizableUI.AREATYPE_TOOLBAR])
        },
        "test-item2": {
          properties: {
            "customizableui-areatypes": CustomizableUI.AREATYPE_TOOLBAR
          },
          expectedSet: new Set([CustomizableUI.AREATYPE_TOOLBAR])
        },
        "test-item3": {
          properties: {
            "customizableui-areatypes": CustomizableUI.AREATYPE_MENU_PANEL
          },
          expectedSet: new Set([CustomizableUI.AREATYPE_MENU_PANEL])
        },
        "test-item4": {
          properties: {
            "customizableui-areatypes": ""
          },
          expectedSet: new Set([CustomizableUI.AREATYPE_TOOLBAR])
        },
        "test-item5": {
          properties: {},
          expectedSet: new Set([CustomizableUI.AREATYPE_TOOLBAR])
        },
        "test-item6": {
          properties: {
            "customizableui-areatypes": [CustomizableUI.AREATYPE_ANY]
          },
          expectedSet: new Set([CustomizableUI.AREATYPE_ANY])
        }
      }

      for (let testItemId in items) {
        let itemProperties = items[testItemId].properties;
        let expectedSet = items[testItemId].expectedSet;
        itemProperties.id = testItemId;

        let node = createToolbarItem(itemProperties);
        addToPalette(node);

        let itemWrapper = CustomizableUI.getWidget(testItemId);
        ok(itemWrapper, "Should be able to retrieve " + testItemId +
                        " via getWidget");
        assertSetsEqual(itemWrapper.allowedAreaTypes, expectedSet);

        // Remove the node from the palette.
        node.parentNode.removeChild(node);
      }
    }
  },
  {
    desc: "Make sure we can read allowedAreaTypes from API widgets",
    run: function() {
      let items = {
        "test-item1": {
          properties: {
            allowedAreaTypes: [
              CustomizableUI.AREATYPE_MENU_PANEL,
              CustomizableUI.AREATYPE_TOOLBAR
            ]
          },
          expectedSet: new Set([CustomizableUI.AREATYPE_MENU_PANEL,
                                CustomizableUI.AREATYPE_TOOLBAR])
        },
        "test-item2": {
          properties: {
            allowedAreaTypes: [
              CustomizableUI.AREATYPE_TOOLBAR
            ]
          },
          expectedSet: new Set([CustomizableUI.AREATYPE_TOOLBAR])
        },
        "test-item3": {
          properties: {
            allowedAreaTypes: [
              CustomizableUI.AREATYPE_MENU_PANEL
            ]
          },
          expectedSet: new Set([CustomizableUI.AREATYPE_MENU_PANEL])
        },
        "test-item4": {
          properties: {
            allowedAreaTypes: []
          },
          expectedSet: new Set([])
        },
        "test-item5": {
          properties: {},
          expectedSet: new Set([CustomizableUI.AREATYPE_TOOLBAR])
        },
        "test-item6": {
          properties: {
            allowedAreaTypes: [
              CustomizableUI.AREATYPE_ANY
            ]
          },
          expectedSet: new Set([CustomizableUI.AREATYPE_ANY])
        }
      }

      for (let testItemId in items) {
        let itemProperties = items[testItemId].properties;
        let expectedSet = items[testItemId].expectedSet;
        itemProperties.id = testItemId;

        let itemWrapper = CustomizableUI.createWidget(itemProperties);
        ok(itemWrapper, "Should have successfully gotten itemWrapper");
        assertSetsEqual(itemWrapper.allowedAreaTypes, expectedSet);

        CustomizableUI.destroyWidget(testItemId);
      }
    }
  },
  {
    desc: "Make sure CustomizableUI.canWidgetMoveToArea is reflecting the allowedAreaTypes",
    run: function() {
      const kPanels = [CustomizableUI.AREA_PANEL];
      const kToolbars = [CustomizableUI.AREA_NAVBAR,
                         CustomizableUI.AREA_MENUBAR,
                         CustomizableUI.AREA_TABSTRIP,
                         CustomizableUI.AREA_BOOKMARKS];
      const kAllAreas = kPanels.concat(kToolbars);

      let items = {
        "test-item1": {
          properties: {
            allowedAreaTypes: [
              CustomizableUI.AREATYPE_MENU_PANEL,
              CustomizableUI.AREATYPE_TOOLBAR
            ]
          },
          allowed: kAllAreas,
          disallowed: [],
        },
        "test-item2": {
          properties: {
            allowedAreaTypes: [
              CustomizableUI.AREATYPE_TOOLBAR
            ]
          },
          allowed: kToolbars,
          disallowed: kPanels,
        },
        "test-item3": {
          properties: {
            allowedAreaTypes: [
              CustomizableUI.AREATYPE_MENU_PANEL
            ]
          },
          allowed: kPanels,
          disallowed: kToolbars
        },
        "test-item4": {
          properties: {
            allowedAreaTypes: []
          },
          allowed: [],
          disallowed: kAllAreas
        },
        "test-item5": {
          properties: {},
          allowed: kToolbars,
          disallowed: kPanels
        },
        "test-item6": {
          properties: {
            allowedAreaTypes: [
              CustomizableUI.AREATYPE_ANY
            ]
          },
          allowed: kAllAreas,
          disallowed: []
        }
      }

      for (let testItemId in items) {
        let itemProperties = items[testItemId].properties;
        let allowed = items[testItemId].allowed;
        let disallowed = items[testItemId].disallowed;
        itemProperties.id = testItemId;

        let itemWrapper = CustomizableUI.createWidget(itemProperties);
        ok(itemWrapper, "Should have successfully gotten itemWrapper");

        for (let areaId of allowed) {
          ok(CustomizableUI.canWidgetMoveToArea(testItemId, areaId),
             testItemId + " should be able to move to area " + areaId);
        }

        for (let areaId of disallowed) {
          is(CustomizableUI.canWidgetMoveToArea(testItemId, areaId), false,
             testItemId + " should not be able to move to area " + areaId);
        }

        CustomizableUI.destroyWidget(testItemId);
      }
    }
  }
];

function cleanup() {
  removeCustomToolbars();
  resetCustomization();
}

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(cleanup);
  runTests(gTests);
}
