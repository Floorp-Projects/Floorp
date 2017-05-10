/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* globals AddonManager */

"use strict";

loader.lazyImporter(this, "AddonManager",
  "resource://gre/modules/AddonManager.jsm");

const { Cc, Ci } = require("chrome");
const { createFactory, createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");
const Services = require("Services");
const AddonsInstallError = createFactory(require("./install-error"));

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const MORE_INFO_URL = "https://developer.mozilla.org/docs/Tools" +
                      "/about:debugging#Enabling_add-on_debugging";

module.exports = createClass({
  displayName: "AddonsControls",

  propTypes: {
    debugDisabled: PropTypes.bool
  },

  getInitialState() {
    return {
      installError: null,
    };
  },

  onEnableAddonDebuggingChange(event) {
    let enabled = event.target.checked;
    Services.prefs.setBoolPref("devtools.chrome.enabled", enabled);
    Services.prefs.setBoolPref("devtools.debugger.remote-enabled", enabled);
  },

  loadAddonFromFile() {
    this.setState({ installError: null });
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window,
      Strings.GetStringFromName("selectAddonFromFile2"),
      Ci.nsIFilePicker.modeOpen);
    fp.open(res => {
      if (res == Ci.nsIFilePicker.returnCancel || !fp.file) {
        return;
      }
      let file = fp.file;
      // AddonManager.installTemporaryAddon accepts either
      // addon directory or final xpi file.
      if (!file.isDirectory() && !file.leafName.endsWith(".xpi")) {
        file = file.parent;
      }

      AddonManager.installTemporaryAddon(file)
        .catch(e => {
          console.error(e);
          this.setState({ installError: e.message });
        });
    });
  },

  render() {
    let { debugDisabled } = this.props;

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
            title: Strings.GetStringFromName("addonDebugging.tooltip")
          }, Strings.GetStringFromName("addonDebugging.label")),
          dom.a({ href: MORE_INFO_URL, target: "_blank" },
            Strings.GetStringFromName("addonDebugging.learnMore")
          ),
        ),
        dom.button({
          id: "load-addon-from-file",
          onClick: this.loadAddonFromFile,
        }, Strings.GetStringFromName("loadTemporaryAddon"))
      ),
      AddonsInstallError({ error: this.state.installError }));
  }
});
