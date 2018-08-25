/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const DebugTargetList = createFactory(require("./DebugTargetList"));

/**
 * This component provides list for debug target and name area.
 */
class DebugTargetPane extends PureComponent {
  static get propTypes() {
    return {
      actionComponent: PropTypes.any.isRequired,
      detailComponent: PropTypes.any.isRequired,
      dispatch: PropTypes.func.isRequired,
      name: PropTypes.string.isRequired,
      targets: PropTypes.arrayOf(PropTypes.Object).isRequired,
    };
  }

  render() {
    const { actionComponent, detailComponent, dispatch, name, targets } = this.props;

    return dom.section(
      {},
      dom.h2({}, name),
      DebugTargetList({ actionComponent, detailComponent, dispatch, targets }),
    );
  }
}

module.exports = DebugTargetPane;
