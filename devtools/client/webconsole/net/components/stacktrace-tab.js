/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PropTypes, createClass, createFactory } = require("devtools/client/shared/vendor/react");
const StackTrace = createFactory(require("devtools/client/shared/components/stack-trace"));

const StackTraceTab = createClass({
  displayName: "StackTraceTab",

  propTypes: {
    data: PropTypes.object.isRequired,
    actions: PropTypes.shape({
      onViewSourceInDebugger: PropTypes.func.isRequired
    })
  },

  render() {
    let { stacktrace } = this.props.data.cause;
    let { actions } = this.props;
    let onViewSourceInDebugger = actions.onViewSourceInDebugger.bind(actions);

    return StackTrace({ stacktrace, onViewSourceInDebugger });
  }
});

// Exports from this module
module.exports = StackTraceTab;
