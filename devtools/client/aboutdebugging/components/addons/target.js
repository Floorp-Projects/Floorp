/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");
const { debugAddon } = require("../../modules/addon");
const Services = require("Services");

loader.lazyImporter(this, "BrowserToolboxProcess",
  "resource://devtools/client/framework/ToolboxProcess.jsm");

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/main", true);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

module.exports = createClass({
  displayName: "AddonTarget",

  propTypes: {
    client: PropTypes.instanceOf(DebuggerClient).isRequired,
    debugDisabled: PropTypes.bool,
    target: PropTypes.shape({
      addonActor: PropTypes.string.isRequired,
      addonID: PropTypes.string.isRequired,
      icon: PropTypes.string,
      name: PropTypes.string.isRequired,
      temporarilyInstalled: PropTypes.bool
    }).isRequired
  },

  debug() {
    let { target } = this.props;
    debugAddon(target.addonID);
  },

  reload() {
    let { client, target } = this.props;
    // This function sometimes returns a partial promise that only
    // implements then().
    client.request({
      to: target.addonActor,
      type: "reload"
    }).then(() => {}, error => {
      throw new Error(
        "Error reloading addon " + target.addonID + ": " + error);
    });
  },

  render() {
    let { target, debugDisabled } = this.props;
    // Only temporarily installed add-ons can be reloaded.
    const canBeReloaded = target.temporarilyInstalled;

    return dom.li({ className: "target-container" },
      dom.img({
        className: "target-icon",
        role: "presentation",
        src: target.icon
      }),
      dom.div({ className: "target" },
        dom.div({ className: "target-name", title: target.name }, target.name)
      ),
      dom.button({
        className: "debug-button",
        onClick: this.debug,
        disabled: debugDisabled,
      }, Strings.GetStringFromName("debug")),
      dom.button({
        className: "reload-button",
        onClick: this.reload,
        disabled: !canBeReloaded,
        title: !canBeReloaded ?
          Strings.GetStringFromName("reloadDisabledTooltip") : ""
      }, Strings.GetStringFromName("reload"))
    );
  }
});
