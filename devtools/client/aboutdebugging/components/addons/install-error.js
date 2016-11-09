/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
"use strict";

const { createClass, DOM: dom, PropTypes } = require("devtools/client/shared/vendor/react");

module.exports = createClass({
  displayName: "AddonsInstallError",

  propTypes: {
    error: PropTypes.string
  },

  render() {
    if (!this.props.error) {
      return null;
    }
    let text = `There was an error during installation: ${this.props.error}`;
    return dom.div({ className: "addons-install-error" },
                   dom.div({ className: "warning" }),
                   dom.span({}, text));
  }
});
