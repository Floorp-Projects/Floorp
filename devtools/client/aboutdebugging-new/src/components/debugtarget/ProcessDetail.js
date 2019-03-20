/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const Types = require("../../types/index");

/**
 * This component displays detail information for a process.
 */
class ProcessDetail extends PureComponent {
  static get propTypes() {
    return {
      target: Types.debugTarget.isRequired,
    };
  }

  render() {
    const { description } = this.props.target.details;
    return dom.p({ className: "debug-target-item__subname ellipsis-text" }, description);
  }
}

module.exports = ProcessDetail;
