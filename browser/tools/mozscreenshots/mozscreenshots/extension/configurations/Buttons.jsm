/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Buttons"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource:///modules/CustomizableUI.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

this.Buttons = {

  init(libDir) {
    createWidget();
  },

  configurations: {
    navBarButtons: {
      applyConfig: Task.async(() => {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        CustomizableUI.addWidgetToArea("screenshot-widget", CustomizableUI.AREA_NAVBAR);
      }),
    },

    tabsToolbarButtons: {
      applyConfig: Task.async(() => {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        CustomizableUI.addWidgetToArea("screenshot-widget", CustomizableUI.AREA_TABSTRIP);
      }),
    },

    menuPanelButtons: {
      applyConfig: Task.async(() => {
        CustomizableUI.addWidgetToArea("screenshot-widget", CustomizableUI.AREA_PANEL);
      }),

      verifyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        if (browserWindow.PanelUI.panel.state == "closed") {
          return Promise.reject("The button isn't shown when the panel isn't open.");
        }
        return Promise.resolve("menuPanelButtons.verifyConfig");
      },
    },

    custPaletteButtons: {
      applyConfig: Task.async(() => {
        CustomizableUI.removeWidgetFromArea("screenshot-widget");
      }),

      verifyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        if (browserWindow.document.documentElement.getAttribute("customizing") != "true") {
          return Promise.reject("The button isn't shown when we're not in customize mode.");
        }
        return Promise.resolve("custPaletteButtons.verifyConfig");
      },
    },
  },
};

function createWidget() {
  let id = "screenshot-widget";
  let spec = {
    id: id,
    label: "My Button",
    removable: true,
    tooltiptext: "",
    type: "button",
  };
  CustomizableUI.createWidget(spec);

  // Append a <style> for the image
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let st = browserWindow.document.createElementNS("http://www.w3.org/1999/xhtml", "style");
  let styles = "" +
        "#screenshot-widget > .toolbarbutton-icon {" +
        "  list-style-image: url(chrome://browser/skin/Toolbar.png);" +
        "  -moz-image-region: rect(0px, 18px, 18px, 0px);" +
        "}";
  st.appendChild(browserWindow.document.createTextNode(styles));
  browserWindow.document.documentElement.appendChild(st);
}
