/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const InspectAction = createFactory(require("./InspectAction"));

const Types = require("../../types/index");

const { getCurrentRuntimeDetails } = require("../../modules/runtimes-state-helper");

/**
 * This component provides components that inspect extension.
 */
class ExtensionAction extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      runtimeDetails: Types.runtimeDetails.isRequired,
      target: Types.debugTarget.isRequired,
    };
  }

  render() {
    const { dispatch, runtimeDetails, target } = this.props;
    const { extensionDebugEnabled } = runtimeDetails;
    return InspectAction({
      disabled: !extensionDebugEnabled,
      dispatch,
      target,
    });
  }
}

const mapStateToProps = state => {
  return {
    runtimeDetails: getCurrentRuntimeDetails(state.runtimes),
  };
};
module.exports = connect(mapStateToProps)(ExtensionAction);
