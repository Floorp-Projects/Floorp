/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Services = require("Services");

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

class AddonsInstallError extends Component {
  static get propTypes() {
    return {
      error: PropTypes.string,
      retryInstall: PropTypes.func,
    };
  }

  render() {
    if (!this.props.error) {
      return null;
    }
    let text = Strings.formatStringFromName("addonInstallError", [this.props.error], 1);
    return dom.div(
      { className: "addons-install-error" },
      dom.span(
        {},
        dom.div({ className: "warning" }),
        dom.span({}, text),
      ),
      dom.button(
        { className: "addons-install-retry", onClick: this.props.retryInstall },
        Strings.GetStringFromName("retryTemporaryInstall")));
  }
}

module.exports = AddonsInstallError;
