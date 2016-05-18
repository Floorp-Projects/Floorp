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
 * - key:
 *    - id:
 *      prefixed by 'key_' to compute <xul:key> id attribute
 *    - modifiers:
 *      optional modifiers for the key shortcut
 *    - keytext:
 *      boolean, to set to true for key shortcut using regular character
 * - additionalKeys:
 *   Array of additional keys, see `key` definition.
 * - disabled:
 *   If true, the menuitem and key shortcut are going to be hidden and disabled
 *   on startup, until some runtime code eventually enable them.
 * - checkbox:
 *   If true, the menuitem is prefixed by a checkbox and runtime code can
 *   toggle it.
 */

const Services = require("Services");
const isMac = Services.appinfo.OS === "Darwin";

loader.lazyRequireGetter(this, "gDevToolsBrowser", "devtools/client/framework/devtools-browser", true);
loader.lazyRequireGetter(this, "Eyedropper", "devtools/client/eyedropper/eyedropper", true);

loader.lazyImporter(this, "BrowserToolboxProcess", "resource://devtools/client/framework/ToolboxProcess.jsm");
loader.lazyImporter(this, "ResponsiveUIManager", "resource://devtools/client/responsivedesign/responsivedesign.jsm");
loader.lazyImporter(this, "ScratchpadManager", "resource://devtools/client/scratchpad/scratchpad-manager.jsm");

exports.menuitems = [
  { id: "menu_devToolbox",
    l10nKey: "devToolboxMenuItem",
    oncommand(event) {
      let window = event.target.ownerDocument.defaultView;
      gDevToolsBrowser.toggleToolboxCommand(window.gBrowser);
    },
    key: {
      id: "devToolboxMenuItem",
      modifiers: isMac ? "accel,alt" : "accel,shift",
      // This is the only one with a letter key
      // and needs to be translated differently
      keytext: true,
    },
    additionalKeys: [{
      id: "devToolboxMenuItemF12",
      l10nKey: "devToolsCmd",
    }],
    checkbox: true
  },
  { id: "menu_devtools_separator",
    separator: true },
  { id: "menu_devToolbar",
    l10nKey: "devToolbarMenu",
    disabled: true,
    oncommand(event) {
      let window = event.target.ownerDocument.defaultView;
      // Distinguish events when selecting a menuitem, where we either open
      // or close the toolbar and when hitting the key shortcut where we just
      // focus the toolbar if it doesn't already has it.
      if (event.target.tagName.toLowerCase() == "menuitem") {
        window.DeveloperToolbar.toggle();
      } else {
        window.DeveloperToolbar.focusToggle();
      }
    },
    key: {
      id: "devToolbar",
      modifiers: "shift"
    },
    checkbox: true
  },
  { id: "menu_webide",
    l10nKey: "webide",
    disabled: true,
    oncommand() {
      gDevToolsBrowser.openWebIDE();
    },
    key: {
      id: "webide",
      modifiers: "shift"
    }
  },
  { id: "menu_browserToolbox",
    l10nKey: "browserToolboxMenu",
    disabled: true,
    oncommand() {
      BrowserToolboxProcess.init();
    },
    key: {
      id: "browserToolbox",
      modifiers: "accel,alt,shift",
      keytext: true
    }
  },
  { id: "menu_browserContentToolbox",
    l10nKey: "browserContentToolboxMenu",
    disabled: true,
    oncommand() {
      gDevToolsBrowser.openContentProcessToolbox();
    }
  },
  { id: "menu_browserConsole",
    l10nKey: "browserConsoleCmd",
    oncommand() {
      let HUDService = require("devtools/client/webconsole/hudservice");
      HUDService.openBrowserConsoleOrFocus();
    },
    key: {
      id: "browserConsole",
      modifiers: "accel,shift",
      keytext: true
    }
  },
  { id: "menu_responsiveUI",
    l10nKey: "responsiveDesignMode",
    oncommand(event) {
      let window = event.target.ownerDocument.defaultView;
      ResponsiveUIManager.toggle(window, window.gBrowser.selectedTab);
    },
    key: {
      id: "responsiveUI",
      modifiers: isMac ? "accel,alt" : "accel,shift",
      keytext: true
    },
    checkbox: true
  },
  { id: "menu_eyedropper",
    l10nKey: "eyedropper",
    oncommand(event) {
      let window = event.target.ownerDocument.defaultView;
      let eyedropper = new Eyedropper(window, { context: "menu",
                                                copyOnSelect: true });
      eyedropper.open();
    },
    checkbox: true
  },
  { id: "menu_scratchpad",
    l10nKey: "scratchpad",
    oncommand() {
      ScratchpadManager.openScratchpad();
    },
    key: {
      id: "scratchpad",
      modifiers: "shift"
    }
  },
  { id: "menu_devtools_serviceworkers",
    l10nKey: "devtoolsServiceWorkers",
    disabled: true,
    oncommand(event) {
      let window = event.target.ownerDocument.defaultView;
      gDevToolsBrowser.openAboutDebugging(window.gBrowser, "workers");
    }
  },
  { id: "menu_devtools_connect",
    l10nKey: "devtoolsConnect",
    disabled: true,
    oncommand(event) {
      let window = event.target.ownerDocument.defaultView;
      gDevToolsBrowser.openConnectScreen(window.gBrowser);
    }
  },
  { separator: true,
    id: "devToolsEndSeparator"
  },
  { id: "getMoreDevtools",
    l10nKey: "getMoreDevtoolsCmd",
    oncommand(event) {
      let window = event.target.ownerDocument.defaultView;
      window.openUILinkIn("https://addons.mozilla.org/firefox/collections/mozilla/webdeveloper/", "tab");
    }
  },
];
