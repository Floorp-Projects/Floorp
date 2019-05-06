/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* globals AddonManager */

"use strict";

loader.lazyImporter(this, "AddonManager",
  "resource://gre/modules/AddonManager.jsm");

const { createFactory, Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { openTemporaryExtension } = require("devtools/client/aboutdebugging/modules/addon");
const Services = require("Services");
const AddonsInstallError = createFactory(require("./InstallError"));

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const MORE_INFO_URL = "https://developer.mozilla.org/docs/Tools" +
                      "/about:debugging#Enabling_add-on_debugging";

class AddonsControls extends Component {
  static get propTypes() {
    return {
      debugDisabled: PropTypes.bool,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      installError: null,
    };

    this.onEnableAddonDebuggingChange = this.onEnableAddonDebuggingChange.bind(this);
    this.loadAddonFromFile = this.loadAddonFromFile.bind(this);
    this.retryInstall = this.retryInstall.bind(this);
    this.installAddon = this.installAddon.bind(this);
  }

  onEnableAddonDebuggingChange(event) {
    const enabled = event.target.checked;
    Services.prefs.setBoolPref("devtools.chrome.enabled", enabled);
    Services.prefs.setBoolPref("devtools.debugger.remote-enabled", enabled);
  }

  async loadAddonFromFile() {
    const message = Strings.GetStringFromName("selectAddonFromFile2");
    const file = await openTemporaryExtension(window, message);
    this.installAddon(file);
  }

  retryInstall() {
    this.setState({ installError: null });
    this.installAddon(this.state.lastInstallErrorFile);
  }

  installAddon(file) {
    AddonManager.installTemporaryAddon(file)
      .then(() => {
        this.setState({ lastInstallErrorFile: null });
      })
      .catch(e => {
        console.error(e);
        this.setState({ installError: e, lastInstallErrorFile: file });
      });
  }

  render() {
    const { debugDisabled } = this.props;
    const isXpinstallEnabled = Services.prefs.getBoolPref("xpinstall.enabled", true);

    return dom.div({ className: "addons-top" },
      dom.div({ className: "addons-controls" },
        dom.div({ className: "addons-options toggle-container-with-text" },
          dom.input({
            id: "enable-addon-debugging",
            type: "checkbox",
            checked: !debugDisabled,
            onChange: this.onEnableAddonDebuggingChange,
            role: "checkbox",
          }),
          dom.label({
            className: "addons-debugging-label",
            htmlFor: "enable-addon-debugging",
            title: Strings.GetStringFromName("addonDebugging.tooltip"),
          }, Strings.GetStringFromName("addonDebugging.label")),
          dom.a({ href: MORE_INFO_URL, target: "_blank" },
            Strings.GetStringFromName("addonDebugging.learnMore")
          ),
        ),
        isXpinstallEnabled ? dom.button({
          id: "load-addon-from-file",
          onClick: this.loadAddonFromFile,
        }, Strings.GetStringFromName("loadTemporaryAddon2")) : null,
      ),
      AddonsInstallError({
        error: this.state.installError,
        retryInstall: this.retryInstall,
      }));
  }
}

module.exports = AddonsControls;
