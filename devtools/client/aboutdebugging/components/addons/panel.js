/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");
const { Management } = require("resource://gre/modules/Extension.jsm");
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
const WEB_EXT_URL = "https://developer.mozilla.org/Add-ons" +
                    "/WebExtensions/Getting_started_with_web-ext";

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
    // Listen to startup since that's when errors and warnings
    // get populated on the extension.
    Management.on("startup", this.updateAddonsList);

    Services.prefs.addObserver(CHROME_ENABLED_PREF,
      this.updateDebugStatus);
    Services.prefs.addObserver(REMOTE_ENABLED_PREF,
      this.updateDebugStatus);

    this.updateDebugStatus();
    this.updateAddonsList();
  },

  componentWillUnmount() {
    AddonManager.removeAddonListener(this);
    Management.off("startup", this.updateAddonsList);

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
            temporarilyInstalled: addon.temporarilyInstalled,
            url: addon.url,
            manifestURL: addon.manifestURL,
            warnings: addon.warnings,
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
    let installedName = Strings.GetStringFromName("extensions");
    let temporaryName = Strings.GetStringFromName("temporaryExtensions");
    let targetClass = AddonTarget;

    const installedTargets = targets.filter((target) => !target.temporarilyInstalled);
    const temporaryTargets = targets.filter((target) => target.temporarilyInstalled);

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
    dom.div({ id: "temporary-addons" },
      TargetList({
        id: "temporary-extensions",
        name: temporaryName,
        targets: temporaryTargets,
        client,
        debugDisabled,
        targetClass,
        sort: true
      }),
      dom.div({ className: "addons-tip"},
        dom.span({
          className: "addons-web-ext-tip",
        }, Strings.GetStringFromName("webExtTip")),
        dom.a({ href: WEB_EXT_URL, target: "_blank" },
          Strings.GetStringFromName("webExtTip.learnMore")
        )
      )
    ),
    dom.div({ id: "addons" },
      TargetList({
        id: "extensions",
        name: installedName,
        targets: installedTargets,
        client,
        debugDisabled,
        targetClass,
        sort: true
      })
    ));
  }
});
