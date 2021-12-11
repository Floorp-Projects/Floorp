/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class RequestListColumnMethod extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item.method !== nextProps.item.method;
  }

  render() {
    const { method } = this.props.item;
    return dom.td(
      { className: "requests-list-column requests-list-method" },
      method
    );
  }
}

module.exports = RequestListColumnMethod;
