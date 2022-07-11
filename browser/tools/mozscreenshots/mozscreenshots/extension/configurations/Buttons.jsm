/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Buttons"];

const { CustomizableUI } = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm"
);

var Buttons = {
  init(libDir) {
    createWidget();
  },

  configurations: {
    navBarButtons: {
      selectors: ["#nav-bar"],
      applyConfig: async () => {
        CustomizableUI.addWidgetToArea(
          "screenshot-widget",
          CustomizableUI.AREA_NAVBAR
        );
      },
    },

    tabsToolbarButtons: {
      selectors: ["#TabsToolbar"],
      applyConfig: async () => {
        CustomizableUI.addWidgetToArea(
          "screenshot-widget",
          CustomizableUI.AREA_TABSTRIP
        );
      },
    },

    menuPanelButtons: {
      selectors: ["#widget-overflow"],
      applyConfig: async () => {
        CustomizableUI.addWidgetToArea(
          "screenshot-widget",
          CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
        );
      },

      async verifyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow(
          "navigator:browser"
        );
        if (browserWindow.PanelUI.panel.state == "closed") {
          return "The button isn't shown when the panel isn't open.";
        }
        return undefined;
      },
    },

    custPaletteButtons: {
      selectors: ["#customization-palette"],
      applyConfig: async () => {
        CustomizableUI.removeWidgetFromArea("screenshot-widget");
      },

      async verifyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow(
          "navigator:browser"
        );
        if (
          browserWindow.document.documentElement.getAttribute("customizing") !=
          "true"
        ) {
          return "The button isn't shown when we're not in customize mode.";
        }
        return undefined;
      },
    },
  },
};

function createWidget() {
  let id = "screenshot-widget";
  let spec = {
    id,
    label: "My Button",
    removable: true,
    tooltiptext: "",
    type: "button",
  };
  CustomizableUI.createWidget(spec);

  // Append a <style> for the image
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let st = browserWindow.document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "style"
  );
  let styles =
    "" +
    "#screenshot-widget > .toolbarbutton-icon {" +
    "  list-style-image: url(chrome://browser/skin/Toolbar.png);" +
    "  -moz-image-region: rect(0px, 18px, 18px, 0px);" +
    "}";
  st.appendChild(browserWindow.document.createTextNode(styles));
  browserWindow.document.documentElement.appendChild(st);
}
