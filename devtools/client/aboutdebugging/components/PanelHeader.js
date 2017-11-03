/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

class PanelHeader extends Component {
  static get propTypes() {
    return {
      id: PropTypes.string.isRequired,
      name: PropTypes.string.isRequired
    };
  }

  render() {
    let { name, id } = this.props;

    return dom.div({ className: "header" },
      dom.h1({ id, className: "header-name" }, name));
  }
}

module.exports = PanelHeader;
