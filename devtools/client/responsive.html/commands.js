/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

loader.lazyRequireGetter(this, "ResponsiveUIManager", "devtools/client/responsive.html/manager", true);

const BRAND_SHORT_NAME = Services.strings.createBundle("chrome://branding/locale/brand.properties")
                                 .GetStringFromName("brandShortName");

const osString = Services.appinfo.OS;
const l10n = require("gcli/l10n");

exports.items = [
  {
    name: "resize",
    description: l10n.lookup("resizeModeDesc")
  },
  {
    item: "command",
    runAt: "client",
    name: "resize on",
    description: l10n.lookup("resizeModeOnDesc"),
    manual: l10n.lookupFormat("resizeModeManual2", [BRAND_SHORT_NAME]),
    exec: resize
  },
  {
    item: "command",
    runAt: "client",
    name: "resize off",
    description: l10n.lookup("resizeModeOffDesc"),
    manual: l10n.lookupFormat("resizeModeManual2", [BRAND_SHORT_NAME]),
    exec: resize
  },
  {
    item: "command",
    runAt: "client",
    name: "resize toggle",
    buttonId: "command-button-responsive",
    buttonClass: "command-button",
    tooltipText: l10n.lookupFormat(
      "resizeModeToggleTooltip2",
      [(osString == "Darwin" ? "Cmd+Opt+M" : "Ctrl+Shift+M")]
    ),
    description: l10n.lookup("resizeModeToggleDesc"),
    manual: l10n.lookupFormat("resizeModeManual2", [BRAND_SHORT_NAME]),
    state: {
      isChecked: function(target) {
        if (!target.tab) {
          return false;
        }
        return ResponsiveUIManager.isActiveForTab(target.tab);
      },
      onChange: function(target, changeHandler) {
        if (target.tab) {
          ResponsiveUIManager.on("on", changeHandler);
          ResponsiveUIManager.on("off", changeHandler);
        }
      },
      offChange: function(target, changeHandler) {
        // Do not check for target.tab as it may already be null during destroy
        ResponsiveUIManager.off("on", changeHandler);
        ResponsiveUIManager.off("off", changeHandler);
      },
    },
    exec: resize
  },
  {
    item: "command",
    runAt: "client",
    name: "resize to",
    description: l10n.lookup("resizeModeToDesc"),
    params: [
      {
        name: "width",
        type: "number",
        description: l10n.lookup("resizePageArgWidthDesc"),
      },
      {
        name: "height",
        type: "number",
        description: l10n.lookup("resizePageArgHeightDesc"),
      },
    ],
    exec: resize
  }
];

async function resize(args, context) {
  const browserWindow = context.environment.chromeWindow;
  await ResponsiveUIManager.handleGcliCommand(browserWindow,
                                              browserWindow.gBrowser.selectedTab,
                                              this.name,
                                              args);
}
