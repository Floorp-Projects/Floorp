/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

// Lists all the properties that plugins.html needs
const NEEDED_PROPS = [
  "name",
  "pluginLibraries",
  "pluginFullpath",
  "version",
  "isActive",
  "blocklistState",
  "description",
];

export class AboutPluginsParent extends JSWindowActorParent {
  async receiveMessage(message) {
    switch (message.name) {
      case "RequestPlugins":
        function filterProperties(plugin) {
          let filtered = {};
          for (let prop of NEEDED_PROPS) {
            filtered[prop] = plugin[prop];
          }
          return filtered;
        }

        let plugins = await lazy.AddonManager.getAddonsByTypes(["plugin"]);
        return plugins.map(filterProperties);
    }

    return undefined;
  }
}
