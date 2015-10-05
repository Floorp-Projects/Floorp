/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global AddonManager, React, TargetListComponent */

"use strict";

loader.lazyRequireGetter(this, "React",
  "resource:///modules/devtools/client/shared/vendor/react.js");
loader.lazyRequireGetter(this, "TargetListComponent",
  "devtools/client/aboutdebugging/components/target-list", true);
loader.lazyRequireGetter(this, "Services");

loader.lazyImporter(this, "AddonManager",
  "resource://gre/modules/AddonManager.jsm");

const ExtensionIcon = "chrome://mozapps/skin/extensions/extensionGeneric.png";
const Strings = Services.strings.createBundle(
  "chrome://browser/locale/devtools/aboutdebugging.properties");

exports.AddonsComponent = React.createClass({
  displayName: "AddonsComponent",

  getInitialState() {
    return {
      extensions: []
    };
  },

  componentDidMount() {
    AddonManager.addAddonListener(this);
    this.update();
  },

  componentWillUnmount() {
    AddonManager.removeAddonListener(this);
  },

  render() {
    let client = this.props.client;
    let targets = this.state.extensions;
    let name = Strings.GetStringFromName("extensions");
    return React.createElement("div", null,
      React.createElement(TargetListComponent, { name, targets, client })
    );
  },

  update() {
    AddonManager.getAllAddons(addons => {
      let extensions = addons.filter(addon => addon.isDebuggable).map(addon => {
        return {
          name: addon.name,
          icon: addon.iconURL || ExtensionIcon,
          type: addon.type,
          addonID: addon.id
        };
      });
      this.setState({ extensions });
    });
  },

  onInstalled() {
    this.update();
  },

  onUninstalled() {
    this.update();
  },

  onEnabled() {
    this.update();
  },

  onDisabled() {
    this.update();
  },
});
