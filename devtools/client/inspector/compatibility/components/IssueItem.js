/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Types = require("../types");

class IssueItem extends PureComponent {
  static get propTypes() {
    return {
      ...Types.issue,
    };
  }

  render() {
    return dom.li(
      { key: `${this.props.property}:${this.props.type}` },
      Object.entries(this.props).map(([key, value]) =>
        dom.div({ key }, `${key}:${value}`)
      )
    );
  }
}

module.exports = IssueItem;
