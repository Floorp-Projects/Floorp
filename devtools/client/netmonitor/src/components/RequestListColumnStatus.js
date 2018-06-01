/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

// Components

const StatusCode = createFactory(require("./StatusCode"));

const { div } = dom;

class RequestListColumnStatus extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  render() {
    const { item } = this.props;

    return (
      div({
        className: "requests-list-column requests-list-status",
      },
      StatusCode({ item }),
    ));
  }
}

module.exports = RequestListColumnStatus;
