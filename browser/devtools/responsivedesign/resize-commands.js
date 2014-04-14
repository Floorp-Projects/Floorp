/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");

const BRAND_SHORT_NAME = Cc["@mozilla.org/intl/stringbundle;1"].
                         getService(Ci.nsIStringBundleService).
                         createBundle("chrome://branding/locale/brand.properties").
                         GetStringFromName("brandShortName");

const gcli = require("gcli/index");

exports.items = [
  {
    name: 'resize',
    description: gcli.lookup('resizeModeDesc')
  },
  {
    name: 'resize on',
    description: gcli.lookup('resizeModeOnDesc'),
    manual: gcli.lookupFormat('resizeModeManual2', [BRAND_SHORT_NAME]),
    exec: gcli_cmd_resize
  },
  {
    name: 'resize off',
    description: gcli.lookup('resizeModeOffDesc'),
    manual: gcli.lookupFormat('resizeModeManual2', [BRAND_SHORT_NAME]),
    exec: gcli_cmd_resize
  },
  {
    name: 'resize toggle',
    buttonId: "command-button-responsive",
    buttonClass: "command-button command-button-invertable",
    tooltipText: gcli.lookup("resizeModeToggleTooltip"),
    description: gcli.lookup('resizeModeToggleDesc'),
    manual: gcli.lookupFormat('resizeModeManual2', [BRAND_SHORT_NAME]),
    state: {
      isChecked: function(aTarget) {
        let browserWindow = aTarget.tab.ownerDocument.defaultView;
        let mgr = browserWindow.ResponsiveUI.ResponsiveUIManager;
        return mgr.isActiveForTab(aTarget.tab);
      },
      onChange: function(aTarget, aChangeHandler) {
        let browserWindow = aTarget.tab.ownerDocument.defaultView;
        let mgr = browserWindow.ResponsiveUI.ResponsiveUIManager;
        mgr.on("on", aChangeHandler);
        mgr.on("off", aChangeHandler);
      },
      offChange: function(aTarget, aChangeHandler) {
        if (aTarget.tab) {
          let browserWindow = aTarget.tab.ownerDocument.defaultView;
          let mgr = browserWindow.ResponsiveUI.ResponsiveUIManager;
          mgr.off("on", aChangeHandler);
          mgr.off("off", aChangeHandler);
        }
      },
    },
    exec: gcli_cmd_resize
  },
  {
    name: 'resize to',
    description: gcli.lookup('resizeModeToDesc'),
    params: [
      {
        name: 'width',
        type: 'number',
        description: gcli.lookup("resizePageArgWidthDesc"),
      },
      {
        name: 'height',
        type: 'number',
        description: gcli.lookup("resizePageArgHeightDesc"),
      },
    ],
    exec: gcli_cmd_resize
  }
];

function gcli_cmd_resize(args, context) {
  let browserWindow = context.environment.chromeWindow;
  let mgr = browserWindow.ResponsiveUI.ResponsiveUIManager;
  mgr.handleGcliCommand(browserWindow,
                        browserWindow.gBrowser.selectedTab,
                        this.name,
                        args);
}
