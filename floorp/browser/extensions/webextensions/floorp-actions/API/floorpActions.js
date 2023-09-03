/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */

this.floorpActions = class extends ExtensionAPI {
  getAPI(context) {
    const EventManager = ExtensionCommon.EventManager;
    
    return {
      floorpActions: {
        async _getCurrentWindow() {
          return Services.wm.getMostRecentWindow("navigator:browser");
        },
        async getExtensionWidgetId(extensionId) {
          return ExtensionCommon.makeWidgetId(extensionId);
        },
        async openInSidebar(sidebarId) {
          let window = await this._getCurrentWindow();
          window.SidebarUI.show(sidebarId);
          console.log(`Open: ${sidebarId}`);
        },
        async closeSidebar() {
          let window = await this._getCurrentWindow();
          window.SidebarUI.hide();
        },
        async openBrowserManagerSidebar() {
          let window = await this._getCurrentWindow();
          if (window.document.getElementById("sidebar-splitter2").getAttribute("hidden") == "true" && window.bmsController.nowPage != null) {
            window.bmsController.controllFunctions.changeVisibleWenpanel();
          }
        },
        async closeBrowserManagerSidebar() {
          let window = await this._getCurrentWindow();
          if (window.document.getElementById("sidebar-splitter2").getAttribute("hidden") == "false" && window.bmsController.nowPage != null) {
            window.bmsController.controllFunctions.changeVisibleWenpanel();
          }
        },
        async changeBrowserManagerSidebarVisibility() {
          let window = await this._getCurrentWindow();
          if(window.bmsController.nowPage != null){
            window.bmsController.controllFunctions.changeVisibleWenpanel();
          }
        },
        async showStatusbar() {
          Services.prefs.setBoolPref("browser.display.statusbar", true);
        },
        async hideStatusbar() {
          Services.prefs.setBoolPref("browser.display.statusbar", false);
        },
        async toggleStatusbar() {
          let pref = Services.prefs.getBoolPref("browser.display.statusbar", false);
          Services.prefs.setBoolPref("browser.display.statusbar", !pref);
        },
      },
    };
  }
};
