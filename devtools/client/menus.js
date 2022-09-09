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
 * - checkbox:
 *   If true, the menuitem is prefixed by a checkbox and runtime code can
 *   toggle it.
 */

const { Cu } = require("chrome");

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  BrowserToolboxLauncher:
    "resource://devtools/client/framework/browser-toolbox/Launcher.sys.mjs",
});

loader.lazyRequireGetter(this, "flags", "devtools/shared/flags");

loader.lazyRequireGetter(
  this,
  "gDevToolsBrowser",
  "devtools/client/framework/devtools-browser",
  true
);
loader.lazyRequireGetter(
  this,
  "ResponsiveUIManager",
  "devtools/client/responsive/manager"
);
loader.lazyRequireGetter(
  this,
  "openDocLink",
  "devtools/client/shared/link",
  true
);
loader.lazyRequireGetter(
  this,
  "CommandsFactory",
  "devtools/shared/commands/commands-factory",
  true
);

loader.lazyRequireGetter(
  this,
  "PICKER_TYPES",
  "devtools/shared/picker-constants"
);

exports.menuitems = [
  {
    id: "menu_devToolbox",
    l10nKey: "webDeveloperToolsMenu",
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
  {
    id: "menu_devtools_remotedebugging",
    l10nKey: "devtoolsRemoteDebugging",
    oncommand(event) {
      const window = event.target.ownerDocument.defaultView;
      gDevToolsBrowser.openAboutDebugging(window.gBrowser);
    },
  },
  {
    id: "menu_browserToolbox",
    l10nKey: "browserToolboxMenu",
    oncommand() {
      lazy.BrowserToolboxLauncher.init();
    },
    keyId: "browserToolbox",
  },
  {
    id: "menu_browserContentToolbox",
    l10nKey: "browserContentToolboxMenu",
    oncommand(event) {
      const window = event.target.ownerDocument.defaultView;
      gDevToolsBrowser.openContentProcessToolbox(window.gBrowser);
    },
  },
  {
    id: "menu_browserConsole",
    l10nKey: "browserConsoleCmd",
    oncommand() {
      const {
        BrowserConsoleManager,
      } = require("devtools/client/webconsole/browser-console-manager");
      BrowserConsoleManager.openBrowserConsoleOrFocus();
    },
    keyId: "browserConsole",
  },
  {
    id: "menu_responsiveUI",
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
  {
    id: "menu_eyedropper",
    l10nKey: "eyedropper",
    async oncommand(event) {
      const window = event.target.ownerDocument.defaultView;

      // The eyedropper might be used without a toolbox, so it should use a
      // dedicated commands instance.
      // See Bug 1701004.
      const commands = await CommandsFactory.forTab(
        window.gBrowser.selectedTab
      );
      await commands.targetCommand.startListening();

      const target = commands.targetCommand.targetFront;
      const inspectorFront = await target.getFront("inspector");

      // If RDM is active, disable touch simulation events if they're enabled.
      // Similarly, enable them when the color picker is done picking.
      if (ResponsiveUIManager.isActiveForTab(target.localTab)) {
        const ui = ResponsiveUIManager.getResponsiveUIForTab(target.localTab);
        await ui.responsiveFront.setElementPickerState(
          true,
          PICKER_TYPES.EYEDROPPER
        );

        inspectorFront.once("color-picked", async () => {
          await ui.responsiveFront.setElementPickerState(
            false,
            PICKER_TYPES.EYEDROPPER
          );
        });

        inspectorFront.once("color-pick-canceled", async () => {
          await ui.responsiveFront.setElementPickerState(
            false,
            PICKER_TYPES.EYEDROPPER
          );
        });
      }

      // Destroy the dedicated commands instance when the color picking is
      // finished.
      inspectorFront.once("color-picked", () => commands.destroy());
      inspectorFront.once("color-pick-canceled", () => commands.destroy());

      inspectorFront.pickColorFromPage({ copyOnSelect: true, fromMenu: true });

      if (flags.testing) {
        // Used in devtools/client/inspector/test/browser_inspector_eyedropper_ruleview.js
        Services.obs.notifyObservers(
          { wrappedJSObject: target },
          "color-picker-command-handled"
        );
      }
    },
    checkbox: true,
  },
  {
    id: "extensionsForDevelopers",
    l10nKey: "extensionsForDevelopersCmd",
    appMenuL10nId: "appmenu-developer-tools-extensions",
    oncommand(event) {
      openDocLink(
        "https://addons.mozilla.org/firefox/collections/mozilla/webdeveloper/"
      );
    },
  },
];
