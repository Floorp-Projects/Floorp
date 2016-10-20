/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");
const { createFactory, createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");
const Services = require("Services");

const AddonsControls = createFactory(require("./controls"));
const AddonTarget = createFactory(require("./target"));
const PanelHeader = createFactory(require("../panel-header"));
const TargetList = createFactory(require("../target-list"));

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/main", true);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const ExtensionIcon = "chrome://mozapps/skin/extensions/extensionGeneric.svg";
const CHROME_ENABLED_PREF = "devtools.chrome.enabled";
const REMOTE_ENABLED_PREF = "devtools.debugger.remote-enabled";

module.exports = createClass({
  displayName: "AddonsPanel",

  propTypes: {
    client: PropTypes.instanceOf(DebuggerClient).isRequired,
    id: PropTypes.string.isRequired
  },

  getInitialState() {
    return {
      extensions: [],
      debugDisabled: false,
    };
  },

  componentDidMount() {
    AddonManager.addAddonListener(this);

    Services.prefs.addObserver(CHROME_ENABLED_PREF,
      this.updateDebugStatus, false);
    Services.prefs.addObserver(REMOTE_ENABLED_PREF,
      this.updateDebugStatus, false);

    this.updateDebugStatus();
    this.updateAddonsList();
  },

  componentWillUnmount() {
    AddonManager.removeAddonListener(this);
    Services.prefs.removeObserver(CHROME_ENABLED_PREF,
      this.updateDebugStatus);
    Services.prefs.removeObserver(REMOTE_ENABLED_PREF,
      this.updateDebugStatus);
  },

  updateDebugStatus() {
    let debugDisabled =
      !Services.prefs.getBoolPref(CHROME_ENABLED_PREF) ||
      !Services.prefs.getBoolPref(REMOTE_ENABLED_PREF);

    this.setState({ debugDisabled });
  },

  updateAddonsList() {
    this.props.client.listAddons()
      .then(({addons}) => {
        let extensions = addons.filter(addon => addon.debuggable).map(addon => {
          return {
            name: addon.name,
            icon: addon.iconURL || ExtensionIcon,
            addonID: addon.id,
            addonActor: addon.actor,
            temporarilyInstalled: addon.temporarilyInstalled
          };
        });

        this.setState({ extensions });
      }, error => {
        throw new Error("Client error while listing addons: " + error);
      });
  },

  /**
   * Mandatory callback as AddonManager listener.
   */
  onInstalled() {
    this.updateAddonsList();
  },

  /**
   * Mandatory callback as AddonManager listener.
   */
  onUninstalled() {
    this.updateAddonsList();
  },

  /**
   * Mandatory callback as AddonManager listener.
   */
  onEnabled() {
    this.updateAddonsList();
  },

  /**
   * Mandatory callback as AddonManager listener.
   */
  onDisabled() {
    this.updateAddonsList();
  },

  render() {
    let { client, id } = this.props;
    let { debugDisabled, extensions: targets } = this.state;
    let name = Strings.GetStringFromName("extensions");
    let targetClass = AddonTarget;

    return dom.div({
      id: id + "-panel",
      className: "panel",
      role: "tabpanel",
      "aria-labelledby": id + "-header"
    },
    PanelHeader({
      id: id + "-header",
      name: Strings.GetStringFromName("addons")
    }),
    AddonsControls({ debugDisabled }),
    dom.div({ id: "addons" },
      TargetList({
        id: "extensions",
        name,
        targets,
        client,
        debugDisabled,
        targetClass,
        sort: true
      })
    ));
  }
});
