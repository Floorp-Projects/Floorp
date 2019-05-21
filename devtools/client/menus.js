/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module defines the sorted list of menuitems inserted into the
 * "Web Developer" menu.
 * It also defines the key shortcuts that relates to them.
 *
 * Various fields are necessary for historical compatiblity with XUL/addons:
 * - id:
 *   used as <xul:menuitem> id attribute
 * - l10nKey:
 *   prefix used to locale localization strings from menus.properties
 * - oncommand:
 *   function called when the menu item or key shortcut are fired
 * - keyId:
 *   Identifier used in devtools/client/devtools-startup.js
 *   Helps figuring out the DOM id for the related <xul:key>
 *   in order to have the key text displayed in menus.
 * - disabled:
 *   If true, the menuitem and key shortcut are going to be hidden and disabled
 *   on startup, until some runtime code eventually enable them.
 * - checkbox:
 *   If true, the menuitem is prefixed by a checkbox and runtime code can
 *   toggle it.
 */

const { Cu } = require("chrome");
const Services = require("Services");

loader.lazyRequireGetter(this, "gDevToolsBrowser", "devtools/client/framework/devtools-browser", true);
loader.lazyRequireGetter(this, "TargetFactory", "devtools/client/framework/target", true);
loader.lazyRequireGetter(this, "ResponsiveUIManager", "devtools/client/responsive.html/manager", true);
loader.lazyRequireGetter(this, "openDocLink", "devtools/client/shared/link", true);

loader.lazyImporter(this, "BrowserToolboxProcess", "resource://devtools/client/framework/ToolboxProcess.jsm");
loader.lazyImporter(this, "ScratchpadManager", "resource://devtools/client/scratchpad/scratchpad-manager.jsm");

const isAboutDebuggingEnabled =
  Services.prefs.getBoolPref("devtools.aboutdebugging.new-enabled", false);
const aboutDebuggingItem = {
  id: "menu_devtools_remotedebugging",
  l10nKey: "devtoolsRemoteDebugging",
  disabled: true,
  oncommand(event) {
    const window = event.target.ownerDocument.defaultView;
    gDevToolsBrowser.openAboutDebugging(window.gBrowser);
  },
};

exports.menuitems = [
  { id: "menu_devToolbox",
    l10nKey: "devToolboxMenuItem",
    async oncommand(event) {
      try {
        const window = event.target.ownerDocument.defaultView;
        await gDevToolsBrowser.toggleToolboxCommand(window.gBrowser, Cu.now());
      } catch (e) {
        console.error(`Exception while opening the toolbox: ${e}\n${e.stack}`);
      }
    },
    keyId: "toggleToolbox",
    checkbox: true,
  },
  { id: "menu_devtools_separator",
    separator: true },
  ...(isAboutDebuggingEnabled ? [aboutDebuggingItem] : []),
  { id: "menu_webide",
    l10nKey: "webide",
    disabled: true,
    oncommand() {
      gDevToolsBrowser.openWebIDE();
    },
    keyId: "webide",
  },
  { id: "menu_browserToolbox",
    l10nKey: "browserToolboxMenu",
    disabled: true,
    oncommand() {
      BrowserToolboxProcess.init();
    },
    keyId: "browserToolbox",
  },
  { id: "menu_browserContentToolbox",
    l10nKey: "browserContentToolboxMenu",
    disabled: true,
    oncommand(event) {
      const window = event.target.ownerDocument.defaultView;
      gDevToolsBrowser.openContentProcessToolbox(window.gBrowser);
    },
  },
  { id: "menu_browserConsole",
    l10nKey: "browserConsoleCmd",
    oncommand() {
      const {HUDService} = require("devtools/client/webconsole/hudservice");
      HUDService.openBrowserConsoleOrFocus();
    },
    keyId: "browserConsole",
  },
  { id: "menu_responsiveUI",
    l10nKey: "responsiveDesignMode",
    oncommand(event) {
      const window = event.target.ownerDocument.defaultView;
      ResponsiveUIManager.toggle(window, window.gBrowser.selectedTab, {
        trigger: "menu",
      });
    },
    keyId: "responsiveDesignMode",
    checkbox: true,
  },
  { id: "menu_eyedropper",
    l10nKey: "eyedropper",
    async oncommand(event) {
      const window = event.target.ownerDocument.defaultView;
      const target = await TargetFactory.forTab(window.gBrowser.selectedTab);
      await target.attach();
    // Temporary fix for bug #1493131 - inspector has a different life cycle
    // than most other fronts because it is closely related to the toolbox.
    // TODO: replace with getFront once inspector is separated from the toolbox
      const inspectorFront = await target.getInspector();
      inspectorFront.pickColorFromPage({copyOnSelect: true, fromMenu: true});
    },
    checkbox: true,
  },
  { id: "menu_scratchpad",
    l10nKey: "scratchpad",
    oncommand() {
      ScratchpadManager.openScratchpad();
    },
    keyId: "scratchpad",
  },
  { id: "menu_devtools_connect",
    l10nKey: "devtoolsConnect",
    disabled: true,
    oncommand(event) {
      const window = event.target.ownerDocument.defaultView;
      gDevToolsBrowser.openConnectScreen(window.gBrowser);
    },
  },
  { separator: true,
    id: "devToolsEndSeparator",
  },
  { id: "getMoreDevtools",
    l10nKey: "getMoreDevtoolsCmd",
    oncommand(event) {
      openDocLink("https://addons.mozilla.org/firefox/collections/mozilla/webdeveloper/");
    },
  },
];
